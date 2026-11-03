#include "qvm/compiler/Compiler.hpp"
#include "qvm/common/FileUtils.hpp"

namespace qvm {

Result<BuildReport> Compiler::compileSourceCode(
    std::string_view cCode,
    CompilerConfig config,
    BuildProgressCallback progressCallback,
    BuildLogCallback logCallback) {

    // 1. Determine temporary scratch directory
    std::filesystem::path scratchDir = config.outputDir.empty()
        ? std::filesystem::current_path() / "build" / "editor_scratch"
        : config.outputDir / "editor_scratch";

    std::filesystem::create_directories(scratchDir);

    // 2. Ensure trailing newline to prevent cpp error "No newline at end of file"
    std::string cleanCode(cCode);
    if (cleanCode.empty() || cleanCode.back() != '\n') {
        cleanCode.push_back('\n');
    }

    // Write g_main.c
    auto srcFile = scratchDir / "g_main.c";
    if (auto err = FileUtils::writeFileText(srcFile, cleanCode); err.isError()) {
        return err;
    }

    // 3. Ensure runtime headers and syscall stubs exist in scratchDir
    auto bgLibH = scratchDir / "bg_lib.h";
    if (std::filesystem::exists("third_party/quake3/code/game/bg_lib.h")) {
        std::filesystem::copy_file("third_party/quake3/code/game/bg_lib.h", bgLibH, std::filesystem::copy_options::overwrite_existing);
    } else if (std::filesystem::exists("examples/sample_game/bg_lib.h")) {
        std::filesystem::copy_file("examples/sample_game/bg_lib.h", bgLibH, std::filesystem::copy_options::overwrite_existing);
    }

    auto bgLibC = scratchDir / "bg_lib.c";
    if (std::filesystem::exists("third_party/quake3/code/game/bg_lib.c")) {
        std::filesystem::copy_file("third_party/quake3/code/game/bg_lib.c", bgLibC, std::filesystem::copy_options::overwrite_existing);
    } else if (std::filesystem::exists("examples/sample_game/bg_lib.c")) {
        std::filesystem::copy_file("examples/sample_game/bg_lib.c", bgLibC, std::filesystem::copy_options::overwrite_existing);
    }

    auto syscallsAsm = scratchDir / "g_syscalls.asm";
    if (std::filesystem::exists("third_party/quake3/code/game/g_syscalls.asm")) {
        std::filesystem::copy_file("third_party/quake3/code/game/g_syscalls.asm", syscallsAsm, std::filesystem::copy_options::overwrite_existing);
    } else if (std::filesystem::exists("examples/sample_game/g_syscalls.asm")) {
        std::filesystem::copy_file("examples/sample_game/g_syscalls.asm", syscallsAsm, std::filesystem::copy_options::overwrite_existing);
    } else {
        std::string stubs = "code\n\nequ trap_Printf -1\nequ trap_Print -1\nequ trap_Error -2\nequ memset -3\nequ memcpy -4\n";
        FileUtils::writeFileText(syscallsAsm, stubs);
    }

    // 4. Resolve includes and sanitize paths
    std::vector<std::filesystem::path> sanitizedIncludes;
    for (auto inc : config.includePaths) {
        if (std::filesystem::is_regular_file(inc)) {
            inc = inc.parent_path();
        }
        if (std::filesystem::exists(inc)) {
            sanitizedIncludes.push_back(inc);
        }
    }

    // Add bundled official Quake 3 headers if available
    for (const auto& relDir : {"third_party/quake3/code/game",
                               "third_party/quake3/code/cgame",
                               "third_party/quake3/code/ui",
                               "third_party/quake3/code/q3_ui",
                               "/Users/haseeb-mir/Documents/Code/quake-3-macos/build/quake3/code/game"}) {
        if (std::filesystem::exists(relDir)) {
            sanitizedIncludes.push_back(relDir);
        }
    }
    config.includePaths = sanitizedIncludes;

    // Ensure -DQ3_VM define
    bool hasQ3Vm = false;
    for (const auto& d : config.preprocessorDefines) {
        if (d == "-DQ3_VM" || d == "Q3_VM") {
            hasQ3Vm = true;
            break;
        }
    }
    if (!hasQ3Vm) {
        config.preprocessorDefines.push_back("-DQ3_VM");
    }

    // 5. Configure compilation parameters
    config.module = ModuleType::Custom;
    config.sourceDir = scratchDir;
    if (config.outputDir.empty()) {
        config.outputDir = scratchDir / "bin";
    }
    if (config.outputName.empty()) {
        config.outputName = "custom.qvm";
    }

    return BuildPipeline::execute(config, progressCallback, logCallback);
}

} // namespace qvm
