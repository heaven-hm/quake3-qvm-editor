#include <gtest/gtest.h>
#include "qvm/common/FileUtils.hpp"
#include "qvm/format/QvmHeader.hpp"
#include "qvm/decompiler/QvmParser.hpp"

TEST(QvmParserTest, InvalidHeaderTooShort) {
    std::vector<uint8_t> shortBytes = { 0x44, 0x14, 0x72, 0x12 };
    auto res = qvm::QvmHeader::parse(shortBytes);
    EXPECT_TRUE(res.hasError());
    EXPECT_EQ(res.error().code(), qvm::ErrorCode::InvalidHeader);
}

TEST(QvmParserTest, InvalidMagic) {
    std::vector<uint8_t> badMagic(32, 0);
    badMagic[0] = 0xAA;
    badMagic[1] = 0xBB;
    auto res = qvm::QvmHeader::parse(badMagic);
    EXPECT_TRUE(res.hasError());
    EXPECT_EQ(res.error().code(), qvm::ErrorCode::InvalidMagic);
}

TEST(QvmParserTest, ValidHeaderVersion1) {
    std::vector<uint8_t> headerBytes(32, 0);
    // Magic: 0x12721444
    headerBytes[0] = 0x44;
    headerBytes[1] = 0x14;
    headerBytes[2] = 0x72;
    headerBytes[3] = 0x12;

    // instruction count: 100
    headerBytes[4] = 100;
    // codeOffset: 32
    headerBytes[8] = 32;
    // codeLength: 400
    headerBytes[12] = 0x90;
    headerBytes[13] = 0x01;
    // dataOffset: 432
    headerBytes[16] = 0xb0;
    headerBytes[17] = 0x01;
    // dataLength: 64
    headerBytes[20] = 64;
    // litLength: 32
    headerBytes[24] = 32;
    // bssLength: 1024
    headerBytes[28] = 0x00;
    headerBytes[29] = 0x04;

    auto res = qvm::QvmHeader::parse(headerBytes);
    ASSERT_TRUE(res.hasValue());
    const auto& h = res.value();
    EXPECT_EQ(h.magic, qvm::kVmMagic);
    EXPECT_EQ(h.instructionCount, 100u);
    EXPECT_EQ(h.codeOffset, 32u);
    EXPECT_EQ(h.codeLength, 400u);
    EXPECT_EQ(h.dataOffset, 432u);
    EXPECT_EQ(h.dataLength, 64u);
    EXPECT_EQ(h.litLength, 32u);
    EXPECT_EQ(h.bssLength, 1024u);
    EXPECT_FALSE(h.isVersion2());
}

TEST(QvmParserTest, ParseRealQvmFileIfPresent) {
    std::filesystem::path testQvm = "tests/data/example_test.qvm";
    if (!std::filesystem::exists(testQvm)) {
        GTEST_SKIP() << "Test QVM file not found at " << testQvm;
    }

    auto parsedRes = qvm::QvmParser::parseFile(testQvm);
    ASSERT_TRUE(parsedRes.hasValue());
    const auto& parsed = parsedRes.value();

    EXPECT_TRUE(parsed.header.isValidMagic());
    EXPECT_GT(parsed.header.instructionCount, 0u);
    EXPECT_EQ(parsed.instructions.size(), parsed.header.instructionCount);
    EXPECT_GT(parsed.sections.code.size(), 0u);
}
