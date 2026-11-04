#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/runtime/QvmRuntime.hpp"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <vector>

namespace {

const char* const kStarterTemplateSource =
R"(#include "bg_lib.h"

// Forward declarations
void printf(const char* fmt, ...);
int fibonacci(int n);

int vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4,
           int arg5, int arg6, int arg7, int arg8, int arg9, int arg10,
           int arg11)
{
    if (command == 0) {
        printf("^2[QVM Editor]^7 Module loaded successfully!\n");
        printf("^3[QVM Editor]^7 fibonacci(7) = %i\n", fibonacci(7));
    } else {
        printf("^1[QVM Editor]^7 Unknown engine command: %i\n", command);
    }
    return 0;
}

void printf(const char* fmt, ...)
{
    va_list argptr;
    char text[1024];
    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);
    trap_Print(text);
}

int fibonacci(int n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}
)";

} // namespace

TEST(Runtime, RunsStarterTemplate) {
    // Build a tiny qagame-style QVM from the editor's starter template (so the
    // test stays self-contained and does not depend on the bundled SDK).
    qvm::CompilerConfig cfg;
    cfg.module = qvm::ModuleType::QaGame;
    cfg.linkFullModule = true;
    cfg.preprocessorDefines = {"-DQ3_VM"};
    cfg.sdkDir = (std::filesystem::current_path() / "third_party/quake3/code").string();
    cfg.outputDir = (std::filesystem::temp_directory_path() / "qvm_test_out").string();
    std::filesystem::create_directories(cfg.outputDir);

    auto res = qvm::Compiler::compileSourceCode(kStarterTemplateSource, cfg);
    ASSERT_TRUE(res.hasValue()) << res.error().message();

    const auto& report = res.value();
    ASSERT_TRUE(report.outputQvmPath.has_extension()) << "no QVM produced";
    ASSERT_TRUE(std::filesystem::is_regular_file(report.outputQvmPath));

    // Run the QVM with the embedded q3vm interpreter + a qagame syscall shim.
    qvm::RuntimeOptions opts;
    opts.module = qvm::RuntimeModule::QaGame;
    opts.command = 0;
    opts.traceSyscalls = true;

    qvm::RuntimeResult r;
    auto runRes = qvm::QvmRuntime::run(report.outputQvmPath, opts,
        [&r](std::string_view line, bool isError) {
            if (isError) r.error += line;
            else r.output.push_back(std::string(line));
        });

    ASSERT_TRUE(runRes.hasValue()) << runRes.error().message();
    const auto& result = runRes.value();

    EXPECT_TRUE(result.success) << "QVM failed: " << result.error;
    EXPECT_FALSE(result.output.empty()) << "no output captured from vmMain";
    EXPECT_EQ(result.trace.empty(), false) << "syscall trace expected with traceSyscalls";

    // A few sanity lines: the starter template prints on command 0 (init).
    bool foundInit = false, foundFib = false;
    for (const auto& s : result.output) {
        if (s.find("fibonacci(7)") != std::string::npos) foundFib = true;
        if (s.find("Module loaded successfully") != std::string::npos) foundInit = true;
    }
    EXPECT_TRUE(foundInit) << "expected VM init print not captured";
    EXPECT_TRUE(foundFib) << "expected fibonacci result not captured";

    // Cleanup the temporary output directory.
    std::filesystem::remove_all(cfg.outputDir);
}

TEST(Runtime, ReportsTrapErrorWithoutCrashingHost) {
    const char* const source = R"(
#include "g_local.h"
int vmMain(int command, int arg0, int arg1, int arg2, int arg3,
           int arg4, int arg5, int arg6, int arg7, int arg8,
           int arg9, int arg10, int arg11) {
    trap_Error("intentional runtime failure\\n");
    return 0;
}
)";

    qvm::CompilerConfig cfg;
    cfg.module = qvm::ModuleType::QaGame;
    cfg.linkFullModule = true;
    cfg.preprocessorDefines = {"-DQ3_VM"};
    cfg.sdkDir = (std::filesystem::current_path() / "third_party/quake3/code").string();
    cfg.outputDir = (std::filesystem::temp_directory_path() / "qvm_runtime_error_out").string();
    std::filesystem::create_directories(cfg.outputDir);

    auto compileRes = qvm::Compiler::compileSourceCode(source, cfg);
    ASSERT_TRUE(compileRes.hasValue()) << compileRes.error().message();

    qvm::RuntimeOptions opts;
    opts.traceSyscalls = true;
    auto runRes = qvm::QvmRuntime::run(compileRes.value().outputQvmPath, opts);
    ASSERT_TRUE(runRes.hasValue()) << runRes.error().message();
    EXPECT_FALSE(runRes.value().success) << (runRes.value().trace.empty() ? "no syscall trace" : runRes.value().trace.front());
    EXPECT_FALSE(runRes.value().error.empty());

    std::filesystem::remove_all(cfg.outputDir);
}

TEST(Runtime, RejectsBadInstructionDuringLoadWithoutTerminatingHost) {
    qvm::CompilerConfig cfg;
    cfg.outputDir = (std::filesystem::temp_directory_path() / "qvm_runtime_bad_instruction_out").string();
    cfg.outputName = "valid.qvm";
    std::filesystem::create_directories(cfg.outputDir);

    auto compileRes = qvm::Compiler::compileSourceCode(
        "int vmMain(int command) { return command; }", cfg);
    ASSERT_TRUE(compileRes.hasValue()) << compileRes.error().message();

    const auto validPath = compileRes.value().outputQvmPath;
    std::ifstream input(validPath, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), {});
    ASSERT_GE(bytes.size(), 12u);
    const size_t codeOffset = static_cast<size_t>(bytes[8]) |
                              (static_cast<size_t>(bytes[9]) << 8) |
                              (static_cast<size_t>(bytes[10]) << 16) |
                              (static_cast<size_t>(bytes[11]) << 24);
    ASSERT_LT(codeOffset, bytes.size());
    bytes[codeOffset] = 0xff;

    const auto invalidPath = std::filesystem::path(cfg.outputDir) / "invalid-opcode.qvm";
    std::ofstream output(invalidPath, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();

    auto runRes = qvm::QvmRuntime::run(invalidPath);
    ASSERT_TRUE(runRes.hasValue()) << runRes.error().message();
    EXPECT_FALSE(runRes.value().success);
    EXPECT_FALSE(runRes.value().error.empty());

    std::filesystem::remove_all(cfg.outputDir);
}
