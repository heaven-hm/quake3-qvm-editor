#include "qvm/compiler/Compiler.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/compiler/ModuleConfig.hpp"

namespace qvm {

namespace {

// Locate a Quake III source root (the directory that contains the module
// sub-folders such as cgame/ + game/ + qcommon/) so an in-memory editor
// source can be linked against the full module implementation.
std::filesystem::path findModuleSourceRoot(ModuleType module, const CompilerConfig& config) {
    if (module == ModuleType::Custom) {
        return {};
    }

    const auto moduleFiles = ModuleConfig::getModuleFileList(module);

    std::vector<std::filesystem::path> candidates;
    auto addWithParents = [&candidates](std::filesystem::path p, int levels) {
        for (int i = 0; i < levels && !p.empty(); ++i) {
            candidates.push_back(p);
            auto parent = p.parent_path();
            if (parent == p) {
                break;
            }
            p = parent;
        }
    };

    addWithParents(config.sdkDir, 3);
    for (const auto& inc : config.includePaths) {
        addWithParents(inc, 3);
    }
    addWithParents(std::filesystem::current_path(), 2);
    candidates.push_back("third_party/quake3/code");

    std::error_code ec;
    auto exeDir = FileUtils::getExecutableDirectory();
    for (auto p = exeDir; !p.empty(); p = p.parent_path()) {
        candidates.push_back(p / "third_party" / "quake3" / "code");
        candidates.push_back(p / "code");
        if (p == p.parent_path()) {
            break;
        }
    }

    for (auto cand : candidates) {
        if (cand.empty() || !std::filesystem::is_directory(cand, ec)) {
            continue;
        }
        auto resolved = ModuleConfig::resolveSourceFiles(cand, moduleFiles);
        if (resolved.empty() || !std::filesystem::exists(resolved.front(), ec)) {
            continue;
        }
        // The full module needs its shared C runtime (bg_lib.c) present.
        if (std::filesystem::exists(cand / "game" / "bg_lib.c", ec) ||
            std::filesystem::exists(cand / "qcommon" / "bg_lib.c", ec)) {
            return cand;
        }
        // The candidate may be the module folder itself (e.g. .../code/cgame).
        auto parent = cand.parent_path();
        if (!parent.empty() && std::filesystem::exists(parent / "game" / "bg_lib.c", ec)) {
            return parent;
        }
    }

    return {};
}

} // anonymous namespace

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

    // 3. Deduce and enforce target module mapping
    // cgame:  source: mod.c, syscall: cg_syscalls.asm, output: cgame.qvm
    // qagame: source: mod.c, syscall: g_syscalls.asm,  output: qagame.qvm
    // ui:     source: mod.c, syscall: ui_syscalls.asm, output: ui.qvm
    if (!config.outputName.empty() && config.module == ModuleType::Custom) {
        auto deduced = ModuleConfig::deduceModuleFromOutputName(config.outputName);
        if (deduced != ModuleType::Custom) {
            config.module = deduced;
        }
    }

    auto mapping = ModuleConfig::getModuleMapping(config.module);
    if (config.outputName.empty() || config.outputName == "custom.qvm") {
        config.outputName = mapping.outputFile;
    }

    // Write source file (mod.c) in system temp scratch directory
    auto srcFile = scratchDir / mapping.sourceFile;
    if (auto err = FileUtils::writeFileText(srcFile, cleanCode); err.isError()) {
        return err;
    }

    // 4. Resolve all Quake 3 include directories (including sibling qcommon, cgame, game, ui, etc.)
    config.includePaths = FileUtils::resolveQuake3IncludeDirs(config.includePaths, config.sdkDir);

    // 4a. Module-aware editor build: when a module is selected and its source
    //     tree is available (via the SDK/includes or the bundled Quake III code),
    //     link the full module and use the editor buffer as the custom vmMain
    //     entry, exactly like the CLI single-file build.
    if (config.linkFullModule && config.module != ModuleType::Custom) {
        auto moduleRoot = findModuleSourceRoot(config.module, config);
        if (!moduleRoot.empty()) {
            config.entryOverrideFile = srcFile;
            config.sourceDir = moduleRoot;
            config.selectedFiles.clear();
            config.cleanIntermediate = true;
            if (config.outputDir.empty()) {
                config.outputDir = std::filesystem::current_path() / "build";
            }
            return BuildPipeline::execute(config, progressCallback, logCallback);
        }
    }

    // Clean up any stale files or old intermediates in scratchDir
    std::filesystem::remove(scratchDir / "g_main.c", ec);
    std::filesystem::remove(scratchDir / "bg_lib.c", ec);
    std::filesystem::remove(scratchDir / "bg_lib.asm", ec);
    std::filesystem::remove_all(scratchDir / "intermediate", ec);

    // Remove non-matching syscall asm files to avoid stale cross-linking
    if (mapping.syscallFile != "g_syscalls.asm") std::filesystem::remove(scratchDir / "g_syscalls.asm", ec);
    if (mapping.syscallFile != "cg_syscalls.asm") std::filesystem::remove(scratchDir / "cg_syscalls.asm", ec);
    if (mapping.syscallFile != "ui_syscalls.asm") std::filesystem::remove(scratchDir / "ui_syscalls.asm", ec);

    // Ensure runtime headers exist in scratchDir
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

    // 5. Ensure matching companion syscall asm exists in scratchDir
    auto syscallsAsm = scratchDir / mapping.syscallFile;
    bool syscallsFound = false;
    for (const auto& inc : config.includePaths) {
        if (std::filesystem::exists(inc / mapping.syscallFile, ec)) {
            std::filesystem::copy_file(inc / mapping.syscallFile, syscallsAsm, std::filesystem::copy_options::overwrite_existing, ec);
            syscallsFound = true;
            break;
        }
    }

    if (!syscallsFound && !config.sdkDir.empty()) {
        for (const auto& sub : {"cgame", "game", "ui", "code/cgame", "code/game", "code/ui", "code/q3_ui"}) {
            auto candidate = config.sdkDir / sub / mapping.syscallFile;
            if (std::filesystem::exists(candidate, ec)) {
                std::filesystem::copy_file(candidate, syscallsAsm, std::filesystem::copy_options::overwrite_existing, ec);
                syscallsFound = true;
                break;
            }
        }
    }

    if (!syscallsFound) {
        for (const auto& bundled : {
            "third_party/quake3/code/cgame/" + mapping.syscallFile,
            "third_party/quake3/code/game/" + mapping.syscallFile,
            "third_party/quake3/code/ui/" + mapping.syscallFile
        }) {
            if (std::filesystem::exists(bundled, ec)) {
                std::filesystem::copy_file(bundled, syscallsAsm, std::filesystem::copy_options::overwrite_existing, ec);
                syscallsFound = true;
                break;
            }
        }
    }

    if (!syscallsFound) {
        // Fallback syscall stubs tailored to the module type
        std::string stubs;
        if (config.module == ModuleType::CGame) {
            stubs = "code\n\n"
                    "equ trap_Print                      -1\n"
                    "equ trap_Error                      -2\n"
                    "equ trap_Milliseconds               -3\n"
                    "equ trap_Cvar_Register              -4\n"
                    "equ trap_Cvar_Update                -5\n"
                    "equ trap_Cvar_Set                   -6\n"
                    "equ trap_Cvar_VariableStringBuffer  -7\n"
                    "equ memset -101\n"
                    "equ memcpy -102\n";
        } else if (config.module == ModuleType::Ui) {
            stubs = "code\n\n"
                    "equ trap_Error                      -1\n"
                    "equ trap_Print                      -2\n"
                    "equ trap_Milliseconds               -3\n"
                    "equ trap_Cvar_Set                   -4\n"
                    "equ trap_Cvar_VariableValue         -5\n"
                    "equ trap_Cvar_VariableStringBuffer  -6\n"
                    "equ memset -101\n"
                    "equ memcpy -102\n";
        } else {
            stubs = "code\n\n"
                    "equ trap_Printf -1\n"
                    "equ trap_Print -1\n"
                    "equ trap_Error -2\n"
                    "equ trap_Milliseconds -3\n"
                    "equ trap_Cvar_Register -4\n"
                    "equ trap_Cvar_Update -5\n"
                    "equ trap_Cvar_Set -6\n"
                    "equ memset -3\n"
                    "equ memcpy -4\n";
        }
        FileUtils::writeFileText(syscallsAsm, stubs);
    }

    // 6. Ensure required preprocessor defines
    auto addDefineIfMissing = [&](const std::string& def) {
        for (const auto& d : config.preprocessorDefines) {
            if (d == def || d == ("-" + def) || ("-D" + d) == def) {
                return;
            }
        }
        config.preprocessorDefines.push_back(def);
    };

    addDefineIfMissing("-DQ3_VM");
    if (config.module == ModuleType::CGame) {
        addDefineIfMissing("-DCGAME");
    }

    // 7. Configure compilation parameters - compile ONLY active source file (mod.c) + companion syscall stubs
    config.sourceDir = scratchDir;
    config.selectedFiles = { srcFile, syscallsAsm };
    if (config.outputDir.empty()) {
        config.outputDir = std::filesystem::current_path() / "build";
    }

    return BuildPipeline::execute(config, progressCallback, logCallback);
}

} // namespace qvm
