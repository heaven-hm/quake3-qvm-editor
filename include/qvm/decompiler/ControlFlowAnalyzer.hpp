#pragma once

#include "qvm/format/QvmInstruction.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvm {

struct BasicBlock {
    uint32_t id = 0;
    uint32_t startIndex = 0;
    uint32_t endIndex = 0;
    std::vector<uint32_t> successors;
    std::vector<uint32_t> predecessors;
};

class ControlFlowAnalyzer {
public:
    static std::set<uint32_t> findJumpTargets(const std::vector<QvmInstruction>& instructions);
    
    static std::unordered_map<uint32_t, std::string> buildLabelMap(
        const std::set<uint32_t>& jumpTargets);

    static std::vector<BasicBlock> analyzeBlocks(
        const std::vector<QvmInstruction>& instructions,
        uint32_t funcStartIndex,
        uint32_t funcEndIndex);
};

} // namespace qvm
