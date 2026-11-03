#include "qvm/decompiler/InstructionDecoder.hpp"
#include <sstream>

namespace qvm {

static int32_t readI32LE(const uint8_t* ptr) {
    return static_cast<int32_t>(
        static_cast<uint32_t>(ptr[0]) |
        (static_cast<uint32_t>(ptr[1]) << 8) |
        (static_cast<uint32_t>(ptr[2]) << 16) |
        (static_cast<uint32_t>(ptr[3]) << 24));
}

Result<std::vector<QvmInstruction>> InstructionDecoder::decode(
    std::span<const uint8_t> codeBytes,
    uint32_t expectedInstructionCount) {

    std::vector<QvmInstruction> instructions;
    instructions.reserve(expectedInstructionCount > 0 ? expectedInstructionCount : 1024);

    size_t cursor = 0;
    uint32_t index = 0;

    while (cursor < codeBytes.size()) {
        if (expectedInstructionCount > 0 && index >= expectedInstructionCount) {
            break;
        }

        uint8_t opByte = codeBytes[cursor];
        if (opByte >= static_cast<uint8_t>(Opcode::MaxOpcodes)) {
            std::stringstream ss;
            ss << "Unknown opcode byte 0x" << std::hex << static_cast<int>(opByte)
               << " at instruction index " << std::dec << index
               << " (offset 0x" << std::hex << cursor << ")";
            return Error(ErrorCode::UnknownOpcode, ss.str());
        }

        Opcode opcode = static_cast<Opcode>(opByte);
        uint8_t paramSize = getOpcodeParamSize(opcode);

        if (cursor + 1 + paramSize > codeBytes.size()) {
            return Error(ErrorCode::TruncatedInstruction,
                         "Truncated instruction operand at offset 0x" + std::to_string(cursor));
        }

        QvmInstruction instr;
        instr.index = index;
        instr.offset = static_cast<uint32_t>(cursor);
        instr.opcode = opcode;
        instr.operandSize = paramSize;

        if (paramSize == 1) {
            instr.operand = static_cast<int32_t>(codeBytes[cursor + 1]);
        } else if (paramSize == 4) {
            instr.operand = readI32LE(&codeBytes[cursor + 1]);
        } else {
            instr.operand = 0;
        }

        instructions.push_back(instr);
        cursor += 1 + paramSize;
        ++index;
    }

    if (expectedInstructionCount > 0 && instructions.size() != expectedInstructionCount) {
        // Warning or note: sometimes compilers produce slight mismatch; if within reasonable range, keep instructions
        if (instructions.empty()) {
            return Error(ErrorCode::InvalidInstructionCount,
                         "Expected " + std::to_string(expectedInstructionCount) +
                         " instructions, but parsed 0.");
        }
    }

    return instructions;
}

} // namespace qvm
