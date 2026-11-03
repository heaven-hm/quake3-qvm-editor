#include <gtest/gtest.h>
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/decompiler/StringExtractor.hpp"

TEST(DecompilerTest, SyscallResolution) {
    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-1, qvm::ModuleType::QaGame), "trap_Printf");
    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-2, qvm::ModuleType::QaGame), "trap_Error");
    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-101, qvm::ModuleType::QaGame), "memset");
    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-102, qvm::ModuleType::QaGame), "memcpy");

    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-1, qvm::ModuleType::CGame), "trap_Print");
    EXPECT_EQ(qvm::FunctionAnalyzer::resolveSyscall(-1, qvm::ModuleType::Ui), "trap_Error");
}

TEST(DecompilerTest, StringExtraction) {
    qvm::QvmSections sections;
    std::string s1 = "Hello Quake 3!";
    std::string s2 = "Second string";

    sections.lit.insert(sections.lit.end(), s1.begin(), s1.end());
    sections.lit.push_back('\0');
    sections.lit.insert(sections.lit.end(), s2.begin(), s2.end());
    sections.lit.push_back('\0');

    auto strings = qvm::StringExtractor::extractAll(sections);
    ASSERT_EQ(strings.size(), 2u);
    EXPECT_EQ(strings[0].text, s1);
    EXPECT_EQ(strings[1].text, s2);
    EXPECT_TRUE(strings[0].inLitSection);
}

TEST(DecompilerTest, PseudocodeContainsDisclaimerNotice) {
    qvm::ParsedQvm parsed;
    parsed.header.instructionCount = 2;
    qvm::QvmInstruction enterInstr;
    enterInstr.opcode = qvm::Opcode::Enter;
    enterInstr.operand = 16;
    enterInstr.operandSize = 4;
    enterInstr.offset = 0;

    qvm::QvmInstruction leaveInstr;
    leaveInstr.opcode = qvm::Opcode::Leave;
    leaveInstr.operand = 0;
    leaveInstr.operandSize = 4;
    leaveInstr.offset = 5;

    parsed.instructions = {enterInstr, leaveInstr};

    qvm::FunctionInfo func;
    func.name = "vmMain";
    func.startIndex = 0;
    func.endIndex = 1;
    func.startOffset = 0;
    func.endOffset = 5;
    func.stackFrameSize = 16;

    std::string code = qvm::PseudocodeGenerator::generate(parsed, {func});
    EXPECT_TRUE(code.find("NOTICE & DISCLAIMER") != std::string::npos);
    EXPECT_TRUE(code.find("reconstructed through reverse-engineering") != std::string::npos);
    EXPECT_TRUE(code.find("Never claim this reconstructed output is identical") != std::string::npos);
    EXPECT_TRUE(code.find("int vmMain") != std::string::npos);
}

#include "qvm/decompiler/Exporter.hpp"

TEST(DecompilerTest, DecompileAndExportRoundTrip) {
    std::filesystem::path testQvm = "tests/data/example_test.qvm";
    if (!std::filesystem::exists(testQvm)) {
        GTEST_SKIP() << "Test QVM file not found at " << testQvm;
    }

    auto parseRes = qvm::QvmParser::parseFile(testQvm);
    ASSERT_TRUE(parseRes.hasValue());

    std::filesystem::path outAsm = "build/test_decompiler_out/test.asm";
    std::filesystem::path outC = "build/test_decompiler_out/test.c";

    auto err = qvm::Exporter::exportBoth(parseRes.value(), outAsm.string(), outC.string(), qvm::ModuleType::QaGame);
    EXPECT_FALSE(err.isError()) << err.message();

    EXPECT_TRUE(std::filesystem::exists(outAsm));
    EXPECT_TRUE(std::filesystem::exists(outC));
    EXPECT_GT(std::filesystem::file_size(outAsm), 0u);
    EXPECT_GT(std::filesystem::file_size(outC), 0u);
}

TEST(DecompilerTest, SingleFunctionInspection) {
    std::filesystem::path testQvm = "tests/data/example_test.qvm";
    if (!std::filesystem::exists(testQvm)) {
        GTEST_SKIP() << "Test QVM file not found at " << testQvm;
    }

    auto parseRes = qvm::QvmParser::parseFile(testQvm);
    ASSERT_TRUE(parseRes.hasValue());
    const auto& parsed = parseRes.value();

    auto functions = qvm::FunctionAnalyzer::analyzeFunctions(parsed.instructions);
    ASSERT_FALSE(functions.empty());

    const auto& vmMain = functions.front();
    EXPECT_EQ(vmMain.name, "vmMain");

    // Test single function pseudocode generation
    std::string funcC = qvm::PseudocodeGenerator::generateSingleFunction(parsed, vmMain, functions);
    EXPECT_TRUE(funcC.find("Function Focus:  vmMain") != std::string::npos);
    EXPECT_TRUE(funcC.find("int vmMain(int arg0, int arg1, int arg2, int arg3)") != std::string::npos);

    // Test single function assembly generation
    std::string funcAsm = qvm::Exporter::generateFunctionDisassembly(parsed, vmMain);
    EXPECT_TRUE(funcAsm.find("Function:     vmMain") != std::string::npos);
    EXPECT_TRUE(funcAsm.find("enter") != std::string::npos);
}
