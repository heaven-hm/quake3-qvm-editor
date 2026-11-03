#include "qvm/decompiler/Exporter.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/decompiler/ControlFlowAnalyzer.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include <iomanip>
#include <sstream>

namespace qvm {

std::string Exporter::generateDisassembly(
    const ParsedQvm& parsedQvm,
    ModuleType moduleType) {

    std::stringstream ss;
    ss << "; ===========================================================================\n"
       << "; Quake 3 QVM Editor - Bytecode Disassembly\n"
       << "; File Size: " << parsedQvm.fileSize << " bytes\n"
       << "; Module:    " << moduleTypeToString(moduleType) << "\n"
       << "; ===========================================================================\n\n";

    ss << ";; --- HEADER ---\n"
       << "; Magic:             0x" << std::hex << parsedQvm.header.magic << "\n"
       << std::dec
       << "; Instruction Count: " << parsedQvm.header.instructionCount << "\n"
       << "; Code Segment:      offset 0x" << std::hex << parsedQvm.header.codeOffset
       << "  length 0x" << parsedQvm.header.codeLength << "\n"
       << "; Data Segment:      offset 0x" << std::hex << parsedQvm.header.dataOffset
       << "  length 0x" << parsedQvm.header.dataLength << "\n"
       << "; Lit Segment:       offset 0x" << std::hex
       << (parsedQvm.header.dataOffset + parsedQvm.header.dataLength)
       << "  length 0x" << parsedQvm.header.litLength << "\n"
       << "; BSS Segment:       length 0x" << std::hex << parsedQvm.header.bssLength << "\n\n";

    // Analyze functions
    auto functions = FunctionAnalyzer::analyzeFunctions(parsedQvm.instructions, moduleType);
    std::unordered_map<uint32_t, std::string> funcStartMap;
    for (const auto& func : functions) {
        funcStartMap[func.startIndex] = func.name;
    }

    // Analyze jump targets
    auto targets = ControlFlowAnalyzer::findJumpTargets(parsedQvm.instructions);
    auto labels = ControlFlowAnalyzer::buildLabelMap(targets);

    // Extract strings
    auto strings = StringExtractor::extractAll(parsedQvm.sections);
    auto stringMap = StringExtractor::buildOffsetMap(strings);

    // Function summary
    ss << ";; --- FUNCTIONS (" << functions.size() << ") ---\n";
    for (const auto& func : functions) {
        ss << "; [0x" << std::hex << std::setw(6) << std::setfill('0') << func.startOffset
           << "] " << std::left << std::setw(30) << std::setfill(' ') << func.name
           << "  Hash: 0x" << std::hex << func.hash
           << (func.isKnown ? " [Matched]" : "") << "\n";
    }
    ss << "\n;; --- CODE SEGMENT ---\n";
    ss << "; INDEX     OFFSET    INSTRUCTION      OPERAND      COMMENT\n";
    ss << "; -------------------------------------------------------------------------\n";

    for (size_t i = 0; i < parsedQvm.instructions.size(); ++i) {
        const auto& instr = parsedQvm.instructions[i];

        // Function start marker
        auto fIt = funcStartMap.find(static_cast<uint32_t>(i));
        if (fIt != funcStartMap.end()) {
            ss << "\n; =========================================================================\n"
               << "; Function: " << fIt->second << "\n"
               << "; =========================================================================\n";
        }

        // Jump target marker
        auto lIt = labels.find(static_cast<uint32_t>(i));
        if (lIt != labels.end()) {
            ss << lIt->second << ":\n";
        }

        // Instruction line
        ss << "  " << std::setw(6) << std::setfill('0') << std::dec << i << "  "
           << "0x" << std::setw(6) << std::setfill('0') << std::hex << instr.offset << "  "
           << std::left << std::setw(14) << std::setfill(' ') << instr.opcodeName();

        if (instr.operandSize > 0) {
            ss << " " << std::right << std::setw(10) << std::dec << instr.operand;
        } else {
            ss << "           ";
        }

        // Comment resolution
        std::string comment;
        if (instr.opcode == Opcode::Const) {
            uint32_t addr = static_cast<uint32_t>(instr.operand);
            auto sIt = stringMap.find(addr);
            if (sIt != stringMap.end()) {
                comment = "\"" + sIt->second + "\"";
            } else if (instr.operand < 0) {
                auto sc = FunctionAnalyzer::resolveSyscall(instr.operand, moduleType);
                if (!sc.empty()) {
                    comment = "syscall: " + std::string(sc);
                }
            } else if (instr.operand > 0 && instr.operand < static_cast<int32_t>(parsedQvm.instructions.size())) {
                auto targetFuncIt = funcStartMap.find(static_cast<uint32_t>(instr.operand));
                if (targetFuncIt != funcStartMap.end()) {
                    comment = "func: " + targetFuncIt->second;
                }
            }
        } else if (isConditionalBranch(instr.opcode)) {
            auto tgtLbl = labels.find(static_cast<uint32_t>(instr.operand));
            if (tgtLbl != labels.end()) {
                comment = "-> " + tgtLbl->second;
            }
        } else if (instr.opcode == Opcode::Call) {
            if (i > 0 && parsedQvm.instructions[i - 1].opcode == Opcode::Const) {
                int32_t target = parsedQvm.instructions[i - 1].operand;
                if (target < 0) {
                    auto sc = FunctionAnalyzer::resolveSyscall(target, moduleType);
                    if (!sc.empty()) comment = "call " + std::string(sc);
                } else {
                    auto fTarget = funcStartMap.find(static_cast<uint32_t>(target));
                    if (fTarget != funcStartMap.end()) {
                        comment = "call " + fTarget->second;
                    }
                }
            }
        }

        if (!comment.empty()) {
            ss << "  ; " << comment;
        }

        ss << "\n";
    }

    // Literals section
    ss << "\n;; --- STRING LITERALS (" << strings.size() << ") ---\n";
    for (const auto& s : strings) {
        ss << "; 0x" << std::hex << std::setw(6) << std::setfill('0') << s.offset
           << ": \"" << s.text << "\"\n";
    }

    return ss.str();
}

std::string Exporter::generateFunctionDisassembly(
    const ParsedQvm& parsedQvm,
    const FunctionInfo& func,
    ModuleType moduleType) {

    std::stringstream ss;
    ss << "; =========================================================================\n"
       << "; Function:     " << func.name << "\n"
       << "; Entry Offset: 0x" << std::hex << func.startOffset << " - 0x" << func.endOffset << "\n"
       << "; Stack Frame:  " << std::dec << func.stackFrameSize << " bytes\n"
       << "; Instructions: " << (func.endIndex - func.startIndex + 1)
       << " (index " << func.startIndex << " - " << func.endIndex << ")\n"
       << "; Signature:    0x" << std::hex << func.hash << std::dec << "\n"
       << "; =========================================================================\n\n";

    auto functions = FunctionAnalyzer::analyzeFunctions(parsedQvm.instructions, moduleType);
    std::unordered_map<uint32_t, std::string> funcStartMap;
    for (const auto& f : functions) {
        funcStartMap[f.startIndex] = f.name;
    }

    auto targets = ControlFlowAnalyzer::findJumpTargets(parsedQvm.instructions);
    auto labels = ControlFlowAnalyzer::buildLabelMap(targets);

    auto strings = StringExtractor::extractAll(parsedQvm.sections);
    auto stringMap = StringExtractor::buildOffsetMap(strings);

    ss << "; INDEX     OFFSET    INSTRUCTION      OPERAND      COMMENT\n";
    ss << "; -------------------------------------------------------------------------\n";

    for (size_t i = func.startIndex; i <= func.endIndex && i < parsedQvm.instructions.size(); ++i) {
        const auto& instr = parsedQvm.instructions[i];

        auto lIt = labels.find(static_cast<uint32_t>(i));
        if (lIt != labels.end()) {
            ss << lIt->second << ":\n";
        }

        ss << "  " << std::setw(6) << std::setfill('0') << std::dec << i << "  "
           << "0x" << std::setw(6) << std::setfill('0') << std::hex << instr.offset << "  "
           << std::left << std::setw(14) << std::setfill(' ') << instr.opcodeName();

        if (instr.operandSize > 0) {
            ss << " " << std::right << std::setw(10) << std::dec << instr.operand;
        } else {
            ss << "           ";
        }

        std::string comment;
        if (instr.opcode == Opcode::Const) {
            uint32_t addr = static_cast<uint32_t>(instr.operand);
            auto sIt = stringMap.find(addr);
            if (sIt != stringMap.end()) {
                comment = "\"" + sIt->second + "\"";
            } else if (instr.operand < 0) {
                auto sc = FunctionAnalyzer::resolveSyscall(instr.operand, moduleType);
                if (!sc.empty()) {
                    comment = "syscall: " + std::string(sc);
                }
            } else if (instr.operand > 0 && instr.operand < static_cast<int32_t>(parsedQvm.instructions.size())) {
                auto targetFuncIt = funcStartMap.find(static_cast<uint32_t>(instr.operand));
                if (targetFuncIt != funcStartMap.end()) {
                    comment = "func: " + targetFuncIt->second;
                }
            }
        } else if (isConditionalBranch(instr.opcode)) {
            auto tgtLbl = labels.find(static_cast<uint32_t>(instr.operand));
            if (tgtLbl != labels.end()) {
                comment = "-> " + tgtLbl->second;
            }
        } else if (instr.opcode == Opcode::Call) {
            if (i > 0 && parsedQvm.instructions[i - 1].opcode == Opcode::Const) {
                int32_t target = parsedQvm.instructions[i - 1].operand;
                if (target < 0) {
                    auto sc = FunctionAnalyzer::resolveSyscall(target, moduleType);
                    if (!sc.empty()) comment = "call " + std::string(sc);
                } else {
                    auto fTarget = funcStartMap.find(static_cast<uint32_t>(target));
                    if (fTarget != funcStartMap.end()) {
                        comment = "call " + fTarget->second;
                    }
                }
            }
        }

        if (!comment.empty()) {
            ss << "  ; " << comment;
        }

        ss << "\n";
    }

    return ss.str();
}

Error Exporter::exportDisassembly(
    const ParsedQvm& parsedQvm,
    const std::filesystem::path& outputPath,
    ModuleType moduleType) {

    std::string disasm = generateDisassembly(parsedQvm, moduleType);
    return FileUtils::writeFileText(outputPath, disasm);
}

Error Exporter::exportPseudocode(
    const ParsedQvm& parsedQvm,
    const std::filesystem::path& outputPath,
    ModuleType moduleType) {

    auto functions = FunctionAnalyzer::analyzeFunctions(parsedQvm.instructions, moduleType);
    std::string pseudocode = PseudocodeGenerator::generate(parsedQvm, functions, moduleType);
    return FileUtils::writeFileText(outputPath, pseudocode);
}

Error Exporter::exportBoth(
    const ParsedQvm& parsedQvm,
    const std::filesystem::path& asmOutputPath,
    const std::filesystem::path& pseudocodeOutputPath,
    ModuleType moduleType) {

    if (!asmOutputPath.empty()) {
        if (auto err = exportDisassembly(parsedQvm, asmOutputPath, moduleType); err.isError()) {
            return err;
        }
    }

    if (!pseudocodeOutputPath.empty()) {
        if (auto err = exportPseudocode(parsedQvm, pseudocodeOutputPath, moduleType); err.isError()) {
            return err;
        }
    }

    return Error::ok();
}

} // namespace qvm
