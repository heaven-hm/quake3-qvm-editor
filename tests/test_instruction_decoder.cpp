#include <gtest/gtest.h>
#include "qvm/format/QvmInstruction.hpp"
#include "qvm/decompiler/InstructionDecoder.hpp"

TEST(InstructionDecoderTest, OpcodeNames) {
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Enter), "enter");
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Leave), "leave");
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Const), "const");
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Call), "call");
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Store4), "store4");
    EXPECT_EQ(qvm::getOpcodeName(qvm::Opcode::Add), "add");
}

TEST(InstructionDecoderTest, OperandSizes) {
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Enter), 4);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Const), 4);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Local), 4);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Eq), 4);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Arg), 1);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Call), 0);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Add), 0);
    EXPECT_EQ(qvm::getOpcodeParamSize(qvm::Opcode::Load4), 0);
}

TEST(InstructionDecoderTest, BranchClassification) {
    EXPECT_TRUE(qvm::isConditionalBranch(qvm::Opcode::Eq));
    EXPECT_TRUE(qvm::isConditionalBranch(qvm::Opcode::Ne));
    EXPECT_TRUE(qvm::isConditionalBranch(qvm::Opcode::LtI));
    EXPECT_FALSE(qvm::isConditionalBranch(qvm::Opcode::Jump));
    EXPECT_TRUE(qvm::isUnconditionalJump(qvm::Opcode::Jump));
    EXPECT_TRUE(qvm::isBranchOrJump(qvm::Opcode::Jump));
    EXPECT_TRUE(qvm::isBranchOrJump(qvm::Opcode::Eq));
    EXPECT_FALSE(qvm::isBranchOrJump(qvm::Opcode::Call));
}

TEST(InstructionDecoderTest, DecodeSyntheticBytecode) {
    // Instruction 0: enter 0x20
    // Instruction 1: const 0x100
    // Instruction 2: leave 0
    std::vector<uint8_t> code = {
        static_cast<uint8_t>(qvm::Opcode::Enter), 0x20, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(qvm::Opcode::Const), 0x00, 0x01, 0x00, 0x00,
        static_cast<uint8_t>(qvm::Opcode::Leave), 0x00, 0x00, 0x00, 0x00
    };

    auto res = qvm::InstructionDecoder::decode(code, 3);
    ASSERT_TRUE(res.hasValue());
    const auto& instructions = res.value();
    ASSERT_EQ(instructions.size(), 3u);

    EXPECT_EQ(instructions[0].opcode, qvm::Opcode::Enter);
    EXPECT_EQ(instructions[0].operand, 0x20);

    EXPECT_EQ(instructions[1].opcode, qvm::Opcode::Const);
    EXPECT_EQ(instructions[1].operand, 0x100);

    EXPECT_EQ(instructions[2].opcode, qvm::Opcode::Leave);
    EXPECT_EQ(instructions[2].operand, 0);
}
