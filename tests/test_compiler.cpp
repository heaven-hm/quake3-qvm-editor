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

TEST(CompilerTest, CompileSourceCodeDirectly) {
    const char* cSource = R"(
#include "bg_lib.h"
int vmMain(int command, int arg0, int arg1, int arg2) {
    return 42;
}
)";

    qvm::CompilerConfig config;
    config.outputDir = "build/test_compiler_out";
    config.outputName = "direct_editor_test.qvm";

    auto res = qvm::Compiler::compileSourceCode(cSource, config);
    ASSERT_TRUE(res.hasValue()) << "Direct compilation failed: " << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_TRUE(std::filesystem::exists(report.outputQvmPath));
    EXPECT_GT(report.qvmFileSize, 0u);

    auto parseRes = qvm::QvmParser::parseFile(report.outputQvmPath);
    ASSERT_TRUE(parseRes.hasValue());
    EXPECT_TRUE(parseRes.value().header.isValidMagic());
}

TEST(CompilerTest, CompileUserTimerSnippetWithGLocalH) {
    const char* cSource = R"(#include "g_local.h"
#include "bg_lib.h"

void printf(const char *fmt, ...);
void initializeTimer(int levelTime);
void updateTimer(int levelTime);

static int startTime;
static int lastMessage;
static int frameCount;
static int initialized;

int vmMain(int command,
           int arg0, int arg1, int arg2, int arg3,
           int arg4, int arg5, int arg6, int arg7,
           int arg8, int arg9, int arg10, int arg11)
{
    switch (command)
    {
        case GAME_INIT:
            initializeTimer(arg0);
            return 0;

        case GAME_RUN_FRAME:
            updateTimer(arg0);
            return 0;

        case GAME_SHUTDOWN:
            printf("^1[Timer]^7 Total frames: %d\n", frameCount);
            initialized = 0;
            return 0;
    }

    return -1;
}

void printf(const char *fmt, ...)
{
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    trap_Print(buffer);
}

void initializeTimer(int levelTime)
{
    startTime = levelTime;
    lastMessage = levelTime;
    frameCount = 0;
    initialized = 1;

    printf("^2[Timer]^7 Started at %d ms\n", levelTime);
}

void updateTimer(int levelTime)
{
    int elapsed;

    if (!initialized)
        return;

    frameCount++;

    elapsed = levelTime - startTime;

    if (levelTime - lastMessage >= 5000)
    {
        printf("^3[Timer]^7 Elapsed: %d seconds | Frames: %d\n",
               elapsed / 1000,
               frameCount);

        lastMessage = levelTime;
    }
})";

    qvm::CompilerConfig config;
    config.outputDir = "build/test_compiler_out";
    config.outputName = "timer_test.qvm";

    auto res = qvm::Compiler::compileSourceCode(cSource, config);
    ASSERT_TRUE(res.hasValue()) << "Timer snippet compilation failed: " << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_TRUE(std::filesystem::exists(report.outputQvmPath));
    EXPECT_GT(report.qvmFileSize, 0u);

    auto parseRes = qvm::QvmParser::parseFile(report.outputQvmPath);
    ASSERT_TRUE(parseRes.hasValue());
    EXPECT_TRUE(parseRes.value().header.isValidMagic());
    EXPECT_GT(parseRes.value().instructions.size(), 0u);
}

