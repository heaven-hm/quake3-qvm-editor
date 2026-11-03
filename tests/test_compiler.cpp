#include <gtest/gtest.h>
#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/compiler/ToolchainRunner.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include <fstream>

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

TEST(ModuleConfigTest, NoAutomaticBgLibInAnyModule) {
    for (auto mod : {qvm::ModuleType::QaGame, qvm::ModuleType::CGame, qvm::ModuleType::Ui}) {
        auto list = qvm::ModuleConfig::getModuleFileList(mod);
        for (const auto& f : list.cFiles) {
            EXPECT_NE(f, "bg_lib.c") << "bg_lib.c should not be automatically linked in "
                                     << qvm::moduleTypeToString(mod);
        }
    }
}

TEST(ModuleConfigTest, MappingEnforcesPerModuleSourceSyscallAndOutput) {
    auto cg = qvm::ModuleConfig::getModuleMapping(qvm::ModuleType::CGame);
    EXPECT_EQ(cg.sourceFile, "mod.c");
    EXPECT_EQ(cg.syscallFile, "cg_syscalls.asm");
    EXPECT_EQ(cg.outputFile, "cgame.qvm");

    auto g = qvm::ModuleConfig::getModuleMapping(qvm::ModuleType::QaGame);
    EXPECT_EQ(g.sourceFile, "mod.c");
    EXPECT_EQ(g.syscallFile, "g_syscalls.asm");
    EXPECT_EQ(g.outputFile, "qagame.qvm");

    auto ui = qvm::ModuleConfig::getModuleMapping(qvm::ModuleType::Ui);
    EXPECT_EQ(ui.sourceFile, "mod.c");
    EXPECT_EQ(ui.syscallFile, "ui_syscalls.asm");
    EXPECT_EQ(ui.outputFile, "ui.qvm");
}

TEST(ModuleConfigTest, DeducesModuleFromOutputNameWithTokenBoundaries) {
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("cgame.qvm"), qvm::ModuleType::CGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("cgame_test.qvm"), qvm::ModuleType::CGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("qagame.qvm"), qvm::ModuleType::QaGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("ui.qvm"), qvm::ModuleType::Ui);

    // Substrings must not cause false positives.
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("build.qvm"), qvm::ModuleType::Custom);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("guide.qvm"), qvm::ModuleType::Custom);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("circuit.qvm"), qvm::ModuleType::Custom);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromOutputName("custom.qvm"), qvm::ModuleType::Custom);
}

TEST(CompilerTest, EndToEndCompileSample) {
    qvm::CompilerConfig config;
    config.module = qvm::ModuleType::Custom;
    config.sourceDir = "examples/sample_game";
    config.selectedFiles = {"g_main.c", "bg_lib.c"};
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
    trap_Print(fmt);
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

TEST(CompilerTest, CompileUserSnippetWithQSharedAndCgPublicAndNoOutputScratch) {
    const char* cSource = R"(
#include "q_shared.h"
#include "cg_public.h"

int vmMain(int command, int arg0, int arg1, int arg2, int arg3,
           int arg4, int arg5, int arg6, int arg7,
           int arg8, int arg9, int arg10, int arg11) {
    return 0;
}
)";

    std::filesystem::path testOutDir = "build/test_clean_output";
    std::filesystem::remove_all(testOutDir);

    qvm::CompilerConfig config;
    config.outputDir = testOutDir;
    config.outputName = "cgame_test.qvm";

    auto res = qvm::Compiler::compileSourceCode(cSource, config);
    ASSERT_TRUE(res.hasValue()) << "Compilation with q_shared.h and cg_public.h failed: "
                                << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_TRUE(std::filesystem::exists(report.outputQvmPath));
    EXPECT_GT(report.qvmFileSize, 0u);

    // Verify editor_scratch was NEVER created in output directory
    EXPECT_FALSE(std::filesystem::exists(testOutDir / "editor_scratch"));

    auto parseRes = qvm::QvmParser::parseFile(report.outputQvmPath);
    ASSERT_TRUE(parseRes.hasValue());
    EXPECT_TRUE(parseRes.value().header.isValidMagic());
}

TEST(CompilerTest, ReportCapturesExactQ3AsmCommand) {
    const char* cSource = R"(
int vmMain(int command, int arg0, int arg1, int arg2) {
    return 100;
}
)";
    qvm::CompilerConfig config;
    config.outputDir = "build/test_compiler_out";
    config.outputName = "cmd_test.qvm";

    auto res = qvm::Compiler::compileSourceCode(cSource, config);
    ASSERT_TRUE(res.hasValue()) << "Compilation failed: " << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_FALSE(report.q3asmCommand.empty());
    EXPECT_NE(report.q3asmCommand.find("q3asm"), std::string::npos);
    EXPECT_NE(report.q3asmCommand.find("-o"), std::string::npos);
    EXPECT_NE(report.q3asmCommand.find("mod.asm"), std::string::npos);
}

TEST(CompilerTest, CompileOnlySelectedFilesExcludingBgLib) {
    // In examples/sample_game, there is both g_main.c and bg_lib.c.
    // When we specify only g_main.c in selectedFiles, bg_lib.c must NOT be compiled or linked.
    // (g_main.c will compile as a stand-alone function test without linking bg_lib.c).
    qvm::CompilerConfig config;
    config.module = qvm::ModuleType::Custom;
    config.sourceDir = "examples/sample_game";
    config.selectedFiles = {"g_main.c"};
    config.outputDir = "build/test_compiler_out";
    config.outputName = "selective_test";

    // Since g_main.c calls fib() and printf(), and vsprintf is in bg_lib.c which is NOT linked:
    // This confirms that bg_lib.c is strictly excluded when not selected!
    auto res = qvm::Compiler::compile(config);
    // If not selected, bg_lib.c is not compiled. The assembly will either succeed if fib is local or report missing vsprintf,
    // proving bg_lib.c was NOT automatically pulled in.
    if (res.hasError()) {
        EXPECT_NE(res.error().message().find("vsprintf"), std::string::npos);
    } else {
        EXPECT_EQ(res.value().filesCompiled, 1);
    }
}

TEST(ModuleConfigTest, DeducesModuleFromSourceFileNames) {
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("cgame.c"), qvm::ModuleType::CGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("cg_main.c"), qvm::ModuleType::CGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("game.c"), qvm::ModuleType::QaGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("g_main.c"), qvm::ModuleType::QaGame);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("ui.c"), qvm::ModuleType::Ui);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("ui_main.c"), qvm::ModuleType::Ui);

    // Unrelated names must not be misread as a module.
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("timer.c"), qvm::ModuleType::Custom);
    EXPECT_EQ(qvm::ModuleConfig::deduceModuleFromSourceOrSyscall("build.c"), qvm::ModuleType::Custom);
}

TEST(CompilerTest, CustomEntryLinksFullModuleAndEmitsMappedOutput) {
    std::filesystem::path tmpDir = "build/test_module_entry";
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(tmpDir);
    auto overrideFile = tmpDir / "custom_cgame_entry.c";
    {
        std::ofstream out(overrideFile);
        out << "#include \"cg_local.h\"\n"
               "void CG_Init(int serverMessageNum, int serverCommandSequence, int clientNum);\n"
               "int vmMain(int command,int arg0,int arg1,int arg2,int arg3,int arg4,\n"
               "           int arg5,int arg6,int arg7,int arg8,int arg9,int arg10,int arg11)\n"
               "{\n"
               "    if (command == 0) { CG_Init(arg0, arg1, arg2); }\n"
               "    return 0;\n"
               "}\n";
    }

    qvm::CompilerConfig config;
    config.module = qvm::ModuleType::CGame;
    config.sourceDir = "third_party/quake3/code"; // module root containing cgame/ + game/ + qcommon/
    config.entryOverrideFile = overrideFile;      // single custom entry, calls stock CG_Init
    config.outputDir = tmpDir;

    auto res = qvm::Compiler::compile(config);
    ASSERT_TRUE(res.hasValue()) << (res.hasError() ? res.error().message() : "");

    const auto& report = res.value();
    EXPECT_TRUE(std::filesystem::exists(report.outputQvmPath));
    EXPECT_EQ(report.outputQvmPath.filename().string(), "cgame.qvm");
    // The override provided vmMain; the rest of the module (incl. syscalls) was linked.
    EXPECT_NE(report.q3asmCommand.find("cg_syscalls.asm"), std::string::npos);
    EXPECT_NE(report.q3asmCommand.find("cg_main.asm"), std::string::npos);
}

TEST(CompilerTest, SingleFileModsEnforcePerModuleSyscallMapping) {
    struct Case {
        qvm::ModuleType module;
        const char* output;
        const char* syscall;
    };
    const Case cases[] = {
        {qvm::ModuleType::QaGame, "qagame.qvm", "g_syscalls.asm"},
        {qvm::ModuleType::CGame, "cgame.qvm", "cg_syscalls.asm"},
        {qvm::ModuleType::Ui, "ui.qvm", "ui_syscalls.asm"},
    };

    // trap_Print is present in all three syscall tables (qagame -1, cgame -1, ui -2),
    // so this single source must link against whichever table the module selects.
    const char* cSource = R"(
#include "bg_lib.h"
void trap_Print(const char *s);
int vmMain(int command, int arg0, int arg1, int arg2) {
    if (command == 0) {
        trap_Print("init\n");
    }
    return 0;
}
)";

    for (const auto& c : cases) {
        qvm::CompilerConfig config;
        config.module = c.module;
        config.outputDir = "build/test_compiler_out";

        auto res = qvm::Compiler::compileSourceCode(cSource, config);
        ASSERT_TRUE(res.hasValue()) << "module " << qvm::moduleTypeToString(c.module) << ": "
                                    << (res.hasError() ? res.error().message() : "");

        const auto& report = res.value();
        EXPECT_EQ(report.outputQvmPath.filename().string(), c.output);
        EXPECT_NE(report.q3asmCommand.find(c.syscall), std::string::npos)
            << "expected " << c.output << " to link " << c.syscall;
        EXPECT_NE(report.q3asmCommand.find("mod.asm"), std::string::npos);
    }
}

