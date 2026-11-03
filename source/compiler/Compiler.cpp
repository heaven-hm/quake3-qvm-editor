#include "qvm/compiler/Compiler.hpp"
#include "qvm/common/FileUtils.hpp"

namespace qvm {

Result<BuildReport> Compiler::compileSourceCode(
    std::string_view cCode,
    CompilerConfig config,
    BuildProgressCallback progressCallback,
    BuildLogCallback logCallback) {

    // 1. Determine temporary scratch directory OUTSIDE user's output directory
    std::filesystem::path scratchDir = std::filesystem::temp_directory_path() / "quake3_qvm_editor_scratch";
    std::error_code ec;
    std::filesystem::create_directories(scratchDir, ec);

    // 2. Ensure trailing newline to prevent cpp error "No newline at end of file"
    std::string cleanCode(cCode);
    if (cleanCode.empty() || cleanCode.back() != '\n') {
        cleanCode.push_back('\n');
    }

    // Write g_main.c in system temp scratch directory
    auto srcFile = scratchDir / "g_main.c";
    if (auto err = FileUtils::writeFileText(srcFile, cleanCode); err.isError()) {
        return err;
    }

    // 3. Resolve all Quake 3 include directories (including sibling qcommon, cgame, game, ui, etc.)
    config.includePaths = FileUtils::resolveQuake3IncludeDirs(config.includePaths);

    // 4. Ensure runtime headers and syscall stubs exist in scratchDir
    auto bgLibH = scratchDir / "bg_lib.h";
    bool bgLibFound = false;
    for (const auto& inc : config.includePaths) {
        if (std::filesystem::exists(inc / "bg_lib.h", ec)) {
            std::filesystem::copy_file(inc / "bg_lib.h", bgLibH, std::filesystem::copy_options::overwrite_existing, ec);
            bgLibFound = true;
            break;
        }
    }
    if (!bgLibFound && std::filesystem::exists("third_party/quake3/code/game/bg_lib.h", ec)) {
        std::filesystem::copy_file("third_party/quake3/code/game/bg_lib.h", bgLibH, std::filesystem::copy_options::overwrite_existing, ec);
    }

    auto bgLibC = scratchDir / "bg_lib.c";
    bool bgLibCFound = false;
    for (const auto& inc : config.includePaths) {
        if (std::filesystem::exists(inc / "bg_lib.c", ec)) {
            std::filesystem::copy_file(inc / "bg_lib.c", bgLibC, std::filesystem::copy_options::overwrite_existing, ec);
            bgLibCFound = true;
            break;
        }
    }
    if (!bgLibCFound && std::filesystem::exists("third_party/quake3/code/game/bg_lib.c", ec)) {
        std::filesystem::copy_file("third_party/quake3/code/game/bg_lib.c", bgLibC, std::filesystem::copy_options::overwrite_existing, ec);
    }

    auto syscallsAsm = scratchDir / "g_syscalls.asm";
    bool syscallsFound = false;
    for (const auto& inc : config.includePaths) {
        if (std::filesystem::exists(inc / "g_syscalls.asm", ec)) {
            std::filesystem::copy_file(inc / "g_syscalls.asm", syscallsAsm, std::filesystem::copy_options::overwrite_existing, ec);
            syscallsFound = true;
            break;
        }
    }
    if (!syscallsFound && std::filesystem::exists("third_party/quake3/code/game/g_syscalls.asm", ec)) {
        std::filesystem::copy_file("third_party/quake3/code/game/g_syscalls.asm", syscallsAsm, std::filesystem::copy_options::overwrite_existing, ec);
    } else if (!syscallsFound) {
        std::string stubs = "code\n\nequ trap_Printf -1\nequ trap_Print -1\nequ trap_Error -2\nequ memset -3\nequ memcpy -4\n";
        FileUtils::writeFileText(syscallsAsm, stubs);
    }

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
        config.outputDir = std::filesystem::current_path() / "build";
    }
    if (config.outputName.empty()) {
        config.outputName = "custom.qvm";
    }

    return BuildPipeline::execute(config, progressCallback, logCallback);
}

} // namespace qvm
