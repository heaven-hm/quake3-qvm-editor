#pragma once

#include "qvm/common/Error.hpp"
#include "qvm/format/QvmInstruction.hpp"
#include <span>
#include <vector>

namespace qvm {

class InstructionDecoder {
public:
    static Result<std::vector<QvmInstruction>> decode(
        std::span<const uint8_t> codeBytes,
        uint32_t expectedInstructionCount);
};

} // namespace qvm
