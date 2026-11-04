
#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/compiler/ModuleConfig.hpp"
#include "qvm/common/FileUtils.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace qvm {

Result<BuildReport> Compiler::compileSourceCode(
    std::string_view cCode,
    CompilerConfig config,
    BuildProgressCallback progressCallback,
    BuildLogCallback logCallback) {

    if (cCode.empty()) {
        return Error(ErrorCode::CompilationFailed,
                     "compileSourceCode: empty source buffer");
    }

    // 1. Resolve Quake III include directories (qcommon, cgame, game, ui ...)
    config.includePaths = FileUtils::resolveQuake3IncludeDirs(
        config.includePaths, config.sdkDir);

    // 2. Write the in-memory source to a temp mod.c so the toolchain can compile
    //    it like any other file on disk.
    const auto scratchDir =
        std::filesystem::temp_directory_path() / "quake3_qvm_editor_scratch";
    std::error_code ec;
    std::filesystem::create_directories(scratchDir, ec);

    std::string cleanCode(cCode);
    if (cleanCode.back() != '\n') {
        cleanCode.push_back('\n');
    }

    auto srcFile = scratchDir / "mod.c";
    if (auto err = FileUtils::writeFileText(srcFile, cleanCode); err.isError()) {
        return err;
    }

    // 3. The BuildPipeline treats config.sourceDir as either a project directory
    //    or a single .c file. For an in-memory compile we point it at the mod.c
    //    file we just wrote.
    //
    //    When linkFullModule is requested we must point sourceDir at a directory
    //    that contains the module's stock sources (cg_main.c, g_main.c, …) so the
    //    pipeline can resolve the full module file list. The SDK directory (or the
    //    module subdirectory inside it) is the natural choice; fall back to the
    //    scratch parent when no SDK is available.
    if (config.linkFullModule && !config.sdkDir.empty()) {
        // Point sourceDir at the SDK code tree so resolveSourceFiles can find the
        // module's stock .c files (cg_main.c, g_main.c, …) via its subdirectory
        // search (cgame/, game/, ui/, …).
        config.sourceDir = config.sdkDir / "code";
        if (!std::filesystem::exists(config.sourceDir)) {
            config.sourceDir = config.sdkDir;
        }
        config.entryOverrideFile = srcFile;
    } else {
        config.sourceDir = srcFile;
    }
    if (config.outputDir.empty()) {
        config.outputDir = scratchDir / "out";
    }

    // 4. Build and return.
    return BuildPipeline::execute(config, progressCallback, logCallback);
}

} // namespace qvm
