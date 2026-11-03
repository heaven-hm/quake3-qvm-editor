#include "qvm/decompiler/ControlFlowAnalyzer.hpp"
#include <iomanip>
#include <sstream>

namespace qvm {

std::set<uint32_t> ControlFlowAnalyzer::findJumpTargets(const std::vector<QvmInstruction>& instructions) {
    std::set<uint32_t> targets;

    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];

        if (isConditionalBranch(instr.opcode)) {
            // Conditional branches specify the target instruction index in their operand
            if (instr.operand >= 0 && static_cast<size_t>(instr.operand) < instructions.size()) {
                targets.insert(static_cast<uint32_t>(instr.operand));
            }
        } else if (instr.opcode == Opcode::Jump) {
            // In vanilla QVM, OP_JUMP pops address from stack (loaded by preceding OP_CONST)
            if (i > 0 && instructions[i - 1].opcode == Opcode::Const) {
                int32_t target = instructions[i - 1].operand;
                if (target >= 0 && static_cast<size_t>(target) < instructions.size()) {
                    targets.insert(static_cast<uint32_t>(target));
                }
            }
        }
    }

    return targets;
}

std::unordered_map<uint32_t, std::string> ControlFlowAnalyzer::buildLabelMap(
    const std::set<uint32_t>& jumpTargets) {

    std::unordered_map<uint32_t, std::string> labels;
    for (uint32_t target : jumpTargets) {
        std::stringstream ss;
        ss << "loc_" << std::setw(6) << std::setfill('0') << std::hex << target;
        labels[target] = ss.str();
    }
    return labels;
}

std::vector<BasicBlock> ControlFlowAnalyzer::analyzeBlocks(
    const std::vector<QvmInstruction>& instructions,
    uint32_t funcStartIndex,
    uint32_t funcEndIndex) {

    std::vector<BasicBlock> blocks;
    if (funcStartIndex > funcEndIndex || funcStartIndex >= instructions.size()) {
        return blocks;
    }

    std::set<uint32_t> leaders;
    leaders.insert(funcStartIndex);

    auto allTargets = findJumpTargets(instructions);
    for (uint32_t target : allTargets) {
        if (target >= funcStartIndex && target <= funcEndIndex) {
            leaders.insert(target);
        }
    }

    for (uint32_t i = funcStartIndex; i <= funcEndIndex; ++i) {
        if (isBranchOrJump(instructions[i].opcode) || instructions[i].opcode == Opcode::Leave) {
            if (i + 1 <= funcEndIndex) {
                leaders.insert(i + 1);
            }
        }
    }

    std::vector<uint32_t> sortedLeaders(leaders.begin(), leaders.end());
    for (size_t k = 0; k < sortedLeaders.size(); ++k) {
        BasicBlock block;
        block.id = static_cast<uint32_t>(k);
        block.startIndex = sortedLeaders[k];
        block.endIndex = (k + 1 < sortedLeaders.size()) ?
            (sortedLeaders[k + 1] - 1) : funcEndIndex;

        // Determine successors
        if (block.endIndex <= funcEndIndex) {
            const auto& lastInstr = instructions[block.endIndex];
            if (isConditionalBranch(lastInstr.opcode)) {
                if (lastInstr.operand >= 0 && static_cast<size_t>(lastInstr.operand) < instructions.size()) {
                    block.successors.push_back(static_cast<uint32_t>(lastInstr.operand));
                }
                if (block.endIndex + 1 <= funcEndIndex) {
                    block.successors.push_back(block.endIndex + 1);
                }
            } else if (lastInstr.opcode == Opcode::Jump) {
                if (block.endIndex > block.startIndex && instructions[block.endIndex - 1].opcode == Opcode::Const) {
                    int32_t target = instructions[block.endIndex - 1].operand;
                    if (target >= 0 && static_cast<size_t>(target) < instructions.size()) {
                        block.successors.push_back(static_cast<uint32_t>(target));
                    }
                }
            } else if (lastInstr.opcode != Opcode::Leave) {
                if (block.endIndex + 1 <= funcEndIndex) {
                    block.successors.push_back(block.endIndex + 1);
                }
            }
        }

        blocks.push_back(block);
    }

    return blocks;
}

} // namespace qvm
