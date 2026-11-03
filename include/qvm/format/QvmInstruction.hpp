#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace qvm {

enum class Opcode : uint8_t {
    Undef = 0,
    Ignore = 1,
    Break = 2,
    Enter = 3,
    Leave = 4,
    Call = 5,
    Push = 6,
    Pop = 7,
    Const = 8,
    Local = 9,
    Jump = 10,
    Eq = 11,
    Ne = 12,
    LtI = 13,
    LeI = 14,
    GtI = 15,
    GeI = 16,
    LtU = 17,
    LeU = 18,
    GtU = 19,
    GeU = 20,
    EqF = 21,
    NeF = 22,
    LtF = 23,
    LeF = 24,
    GtF = 25,
    GeF = 26,
    Load1 = 27,
    Load2 = 28,
    Load4 = 29,
    Store1 = 30,
    Store2 = 31,
    Store4 = 32,
    Arg = 33,
    BlockCopy = 34,
    Sex8 = 35,
    Sex16 = 36,
    NegI = 37,
    Add = 38,
    Sub = 39,
    DivI = 40,
    DivU = 41,
    ModI = 42,
    ModU = 43,
    MulI = 44,
    MulU = 45,
    BAnd = 46,
    BOr = 47,
    BXor = 48,
    BCom = 49,
    Lsh = 50,
    RshI = 51,
    RshU = 52,
    NegF = 53,
    AddF = 54,
    SubF = 55,
    DivF = 56,
    MulF = 57,
    CvIF = 58,
    CvFI = 59,
    MaxOpcodes = 60
};

struct QvmInstruction {
    uint32_t index = 0;
    uint32_t offset = 0;
    Opcode opcode = Opcode::Undef;
    int32_t operand = 0;
    uint8_t operandSize = 0;
    std::string comment;

    [[nodiscard]] std::string_view opcodeName() const noexcept;
    [[nodiscard]] std::string formatAssembly() const;
};

std::string_view getOpcodeName(Opcode op) noexcept;
uint8_t getOpcodeParamSize(Opcode op) noexcept;
bool isConditionalBranch(Opcode op) noexcept;
bool isUnconditionalJump(Opcode op) noexcept;
bool isBranchOrJump(Opcode op) noexcept;

} // namespace qvm
