#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/ControlFlowAnalyzer.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include <iomanip>
#include <sstream>

namespace qvm {

namespace {

class DecompileStack {
public:
    void push(std::string expr) {
        stack_.push_back(std::move(expr));
    }

    std::string pop() {
        if (stack_.empty()) {
            return "0";
        }
        std::string top = std::move(stack_.back());
        stack_.pop_back();
        return top;
    }

    [[nodiscard]] bool empty() const noexcept { return stack_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return stack_.size(); }
    void clear() noexcept { stack_.clear(); }

    [[nodiscard]] const std::string& top() const {
        static const std::string empty;
        return stack_.empty() ? empty : stack_.back();
    }

private:
    std::vector<std::string> stack_;
};

std::string escapeString(std::string_view s) {
    std::string res;
    res.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\n': res += "\\n"; break;
            case '\r': res += "\\r"; break;
            case '\t': res += "\\t"; break;
            case '\"': res += "\\\""; break;
            case '\\': res += "\\\\"; break;
            default:
                if (std::isprint(static_cast<unsigned char>(c))) {
                    res += c;
                } else {
                    std::stringstream ss;
                    ss << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                       << (static_cast<int>(c) & 0xff);
                    res += ss.str();
                }
                break;
        }
    }
    return res;
}

} // anonymous namespace

std::string PseudocodeGenerator::generateFunction(
    const ParsedQvm& parsedQvm,
    const FunctionInfo& func,
    const std::unordered_map<uint32_t, std::string>& labels,
    const std::unordered_map<uint32_t, std::string>& stringMap,
    const std::vector<FunctionInfo>& allFunctions,
    ModuleType moduleType) {

    std::stringstream ss;
    ss << "// -------------------------------------------------------------------------\n";
    ss << "// Function: " << func.name
       << " [0x" << std::hex << func.startOffset << " - 0x" << func.endOffset << "]\n";
    ss << "// Stack Frame: " << std::dec << func.stackFrameSize << " bytes"
       << ", Hash: 0x" << std::hex << func.hash << std::dec << "\n";
    ss << "// -------------------------------------------------------------------------\n";

    // Signature
    ss << "int " << func.name << "(int arg0, int arg1, int arg2, int arg3) {\n";

    DecompileStack stack;
    std::vector<std::string> pendingArgs;

    for (uint32_t i = func.startIndex; i <= func.endIndex && i < parsedQvm.instructions.size(); ++i) {
        const auto& instr = parsedQvm.instructions[i];

        // Check if this instruction has an incoming jump label
        auto lblIt = labels.find(i);
        if (lblIt != labels.end()) {
            ss << "\n" << lblIt->second << ":\n";
        }

        switch (instr.opcode) {
            case Opcode::Enter: {
                // Stack frame allocation already covered in signature
                break;
            }
            case Opcode::Leave: {
                if (!stack.empty()) {
                    ss << "    return " << stack.pop() << ";\n";
                } else {
                    ss << "    return 0;\n";
                }
                break;
            }
            case Opcode::Const: {
                uint32_t addr = static_cast<uint32_t>(instr.operand);
                auto strIt = stringMap.find(addr);
                if (strIt != stringMap.end()) {
                    stack.push("\"" + escapeString(strIt->second) + "\"");
                } else if (instr.operand > 0 && instr.operand < static_cast<int32_t>(parsedQvm.instructions.size())) {
                    // Check if it's a function reference
                    bool isFunc = false;
                    for (const auto& otherFunc : allFunctions) {
                        if (otherFunc.startIndex == static_cast<uint32_t>(instr.operand)) {
                            stack.push(otherFunc.name);
                            isFunc = true;
                            break;
                        }
                    }
                    if (!isFunc) {
                        if (instr.operand > 0x1000) {
                            std::stringstream hexVal;
                            hexVal << "0x" << std::hex << instr.operand;
                            stack.push(hexVal.str());
                        } else {
                            stack.push(std::to_string(instr.operand));
                        }
                    }
                } else if (instr.operand < 0) {
                    auto scName = FunctionAnalyzer::resolveSyscall(instr.operand, moduleType);
                    if (!scName.empty()) {
                        stack.push(std::string(scName));
                    } else {
                        stack.push("trap_" + std::to_string(-instr.operand));
                    }
                } else {
                    if (instr.operand > 0x1000) {
                        std::stringstream hexVal;
                        hexVal << "0x" << std::hex << instr.operand;
                        stack.push(hexVal.str());
                    } else {
                        stack.push(std::to_string(instr.operand));
                    }
                }
                break;
            }
            case Opcode::Local: {
                std::stringstream loc;
                if (instr.operand >= 8 && instr.operand <= 20) {
                    int argNum = (instr.operand - 8) / 4;
                    loc << "arg" << argNum;
                } else {
                    loc << "local_" << std::hex << instr.operand;
                }
                stack.push(loc.str());
                break;
            }
            case Opcode::Load1: {
                auto addr = stack.pop();
                stack.push("*(uint8_t*)(" + addr + ")");
                break;
            }
            case Opcode::Load2: {
                auto addr = stack.pop();
                stack.push("*(uint16_t*)(" + addr + ")");
                break;
            }
            case Opcode::Load4: {
                auto addr = stack.pop();
                stack.push("*(int*)(" + addr + ")");
                break;
            }
            case Opcode::Store1: {
                auto val = stack.pop();
                auto dest = stack.pop();
                ss << "    *(uint8_t*)(" << dest << ") = (uint8_t)(" << val << ");\n";
                break;
            }
            case Opcode::Store2: {
                auto val = stack.pop();
                auto dest = stack.pop();
                ss << "    *(uint16_t*)(" << dest << ") = (uint16_t)(" << val << ");\n";
                break;
            }
            case Opcode::Store4: {
                auto val = stack.pop();
                auto dest = stack.pop();
                ss << "    " << dest << " = " << val << ";\n";
                break;
            }
            case Opcode::Arg: {
                auto val = stack.pop();
                pendingArgs.push_back(val);
                break;
            }
            case Opcode::Call: {
                auto target = stack.pop();
                std::string callExpr = target + "(";
                for (size_t a = 0; a < pendingArgs.size(); ++a) {
                    if (a > 0) callExpr += ", ";
                    callExpr += pendingArgs[a];
                }
                callExpr += ")";
                pendingArgs.clear();

                // Check if next instruction ignores return or pushes
                if (i + 1 <= func.endIndex && parsedQvm.instructions[i + 1].opcode == Opcode::Pop) {
                    ss << "    " << callExpr << ";\n";
                } else {
                    stack.push(callExpr);
                }
                break;
            }
            case Opcode::BlockCopy: {
                auto src = stack.pop();
                auto dest = stack.pop();
                ss << "    memcpy(" << dest << ", " << src << ", " << instr.operand << ");\n";
                break;
            }
            case Opcode::Jump: {
                auto target = stack.pop();
                ss << "    goto " << target << ";\n";
                break;
            }
            case Opcode::Eq: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if (" << r1 << " == " << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::Ne: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if (" << r1 << " != " << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::LtI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if ((int)" << r1 << " < (int)" << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::LeI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if ((int)" << r1 << " <= (int)" << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::GtI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if ((int)" << r1 << " > (int)" << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::GeI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                std::string targetLabel = "loc_" + std::to_string(instr.operand);
                auto it = labels.find(static_cast<uint32_t>(instr.operand));
                if (it != labels.end()) targetLabel = it->second;
                ss << "    if ((int)" << r1 << " >= (int)" << r0 << ") goto " << targetLabel << ";\n";
                break;
            }
            case Opcode::Add: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " + " + r0 + ")");
                break;
            }
            case Opcode::Sub: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " - " + r0 + ")");
                break;
            }
            case Opcode::MulI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " * " + r0 + ")");
                break;
            }
            case Opcode::DivI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " / " + r0 + ")");
                break;
            }
            case Opcode::ModI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " % " + r0 + ")");
                break;
            }
            case Opcode::BAnd: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " & " + r0 + ")");
                break;
            }
            case Opcode::BOr: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " | " + r0 + ")");
                break;
            }
            case Opcode::BXor: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " ^ " + r0 + ")");
                break;
            }
            case Opcode::Lsh: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " << " + r0 + ")");
                break;
            }
            case Opcode::RshI: {
                auto r0 = stack.pop();
                auto r1 = stack.pop();
                stack.push("(" + r1 + " >> " + r0 + ")");
                break;
            }
            case Opcode::NegI: {
                auto v = stack.pop();
                stack.push("(-" + v + ")");
                break;
            }
            case Opcode::Pop: {
                if (!stack.empty()) {
                    auto val = stack.pop();
                    if (val.find('(') != std::string::npos) {
                        ss << "    " << val << ";\n";
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    ss << "}\n\n";
    return ss.str();
}

std::string PseudocodeGenerator::generate(
    const ParsedQvm& parsedQvm,
    const std::vector<FunctionInfo>& functions,
    ModuleType moduleType) {

    std::stringstream ss;

    // Standard disclaimer header
    ss << "/* =========================================================================\n"
       << " * Quake 3 QVM Studio - Reconstructed C-Like Pseudocode\n"
       << " * \n"
       << " * Target Module:   " << moduleTypeToString(moduleType) << "\n"
       << " * Instruction Count: " << parsedQvm.header.instructionCount << "\n"
       << " * Total Functions: " << functions.size() << "\n"
       << " * \n"
       << " * NOTICE & DISCLAIMER:\n"
       << " * This pseudocode is reconstructed through reverse-engineering analysis\n"
       << " * of Quake III Virtual Machine bytecode. Due to compiler optimizations,\n"
       << " * stack register lowering, and type erasure, variable names and types are\n"
       << " * inferred. This output is intended for inspection and mod analysis only.\n"
       << " * Never claim this reconstructed output is identical to original C source.\n"
       << " * ========================================================================= */\n\n";

    ss << "#include <stdint.h>\n";
    ss << "#include <string.h>\n\n";

    // Extract strings
    auto strings = StringExtractor::extractAll(parsedQvm.sections);
    auto stringMap = StringExtractor::buildOffsetMap(strings);

    // Analyze labels
    auto targets = ControlFlowAnalyzer::findJumpTargets(parsedQvm.instructions);
    auto labels = ControlFlowAnalyzer::buildLabelMap(targets);

    // Forward declarations
    ss << "// Forward Declarations\n";
    for (const auto& func : functions) {
        ss << "int " << func.name << "(int arg0, int arg1, int arg2, int arg3);\n";
    }
    ss << "\n";

    // Generate each function
    for (const auto& func : functions) {
        ss << generateFunction(parsedQvm, func, labels, stringMap, functions, moduleType);
    }

    return ss.str();
}

} // namespace qvm
