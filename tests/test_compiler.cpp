#include <gtest/gtest.h>
#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/compiler/ToolchainRunner.hpp"
#include "qvm/decompiler/QvmParser.hpp"

TEST(CompilerConfigTest, ModuleTypeConversion) {
    EXPECT_EQ(qvm::moduleTypeToString(qvm::ModuleType::QaGame), "qagame");
    EXPECT_EQ(qvm::moduleTypeToString(qvm::ModuleType::CGame), "cgame");
    EXPECT_EQ(qvm::moduleTypeToString(qvm::ModuleType::Ui), "ui");
    EXPECT_EQ(qvm::moduleTypeToString(qvm::ModuleType::Custom), "custom");

    EXPECT_EQ(qvm::stringToModuleType("qagame"), qvm::ModuleType::QaGame);
    EXPECT_EQ(qvm::stringToModuleType("game"), qvm::ModuleType::QaGame);
    EXPECT_EQ(qvm::stringToModuleType("cgame"), qvm::ModuleType::CGame);
    EXPECT_EQ(qvm::stringToModuleType("ui"), qvm::ModuleType::Ui);
    EXPECT_EQ(qvm::stringToModuleType("q3_ui"), qvm::ModuleType::Ui);
}

TEST(ModuleConfigTest, QaGameFileListGuaranteesVmMainFirst) {
    auto fileList = qvm::ModuleConfig::getModuleFileList(qvm::ModuleType::QaGame);
    EXPECT_EQ(fileList.entryPointFile, "g_main.c");
    EXPECT_EQ(fileList.defaultOutputBaseName, "qagame");
    EXPECT_FALSE(fileList.cFiles.empty());
    EXPECT_FALSE(fileList.asmFiles.empty());
    EXPECT_EQ(fileList.asmFiles[0], "g_syscalls.asm");
}

TEST(ModuleConfigTest, CGameFileListGuaranteesCgMainFirst) {
    auto fileList = qvm::ModuleConfig::getModuleFileList(qvm::ModuleType::CGame);
    EXPECT_EQ(fileList.entryPointFile, "cg_main.c");
    EXPECT_EQ(fileList.defaultOutputBaseName, "cgame");
    EXPECT_FALSE(fileList.cFiles.empty());
}

TEST(ModuleConfigTest, UiFileListGuaranteesUiMainFirst) {
    auto fileList = qvm::ModuleConfig::getModuleFileList(qvm::ModuleType::Ui);
    EXPECT_EQ(fileList.entryPointFile, "ui_main.c");
    EXPECT_EQ(fileList.defaultOutputBaseName, "ui");
    EXPECT_FALSE(fileList.cFiles.empty());
}

TEST(CompilerTest, EndToEndCompileSample) {
    qvm::CompilerConfig config;
    config.module = qvm::ModuleType::Custom;
    config.sourceDir = "examples/sample_game";
    config.outputDir = "build/test_compiler_out";
    config.outputName = "test_sample";

    auto res = qvm::Compiler::compile(config);
    ASSERT_TRUE(res.hasValue()) << "Compilation failed: " << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_TRUE(std::filesystem::exists(report.outputQvmPath));
    EXPECT_GT(report.qvmFileSize, 0u);

    // Verify parsed output
    auto parseRes = qvm::QvmParser::parseFile(report.outputQvmPath);
    ASSERT_TRUE(parseRes.hasValue());
    EXPECT_TRUE(parseRes.value().header.isValidMagic());
    EXPECT_GT(parseRes.value().instructions.size(), 0u);
}
