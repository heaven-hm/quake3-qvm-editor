#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace qvm {

enum class ModuleType {
    QaGame,
    CGame,
    Ui,
    Custom
};

std::string_view moduleTypeToString(ModuleType type) noexcept;
ModuleType stringToModuleType(std::string_view str) noexcept;

struct CompilerConfig {
    ModuleType module = ModuleType::QaGame;
    std::filesystem::path sourceDir;
    std::vector<std::filesystem::path> selectedFiles; // Explicit source files (.c/.asm) to compile & link (if non-empty, only these are compiled)
    std::filesystem::path outputDir;
    std::string outputName; // e.g. "qagame.qvm", if empty auto-determined by module
    std::filesystem::path sdkDir; // optional path to Quake III SDK root/code
    std::vector<std::filesystem::path> includePaths;
    std::vector<std::string> preprocessorDefines;
    std::filesystem::path toolchainDir; // if empty, auto-detect
    // When set, this file is used as the module's custom vmMain entry point. The
    // stock module entry (e.g. cg_main.c) is still linked for its other symbols,
    // but its vmMain is renamed so the override becomes the QVM entry.
    std::filesystem::path entryOverrideFile;
    // Set by the GUI when a concrete module is selected: compileSourceCode() then
    // links the full module implementation (like the CLI single-file build) and
    // uses the editor buffer as the custom vmMain entry.
    bool linkFullModule = false;
    bool debugSymbols = false;
    bool optimize = true;
    bool verbose = false;
    bool cleanIntermediate = false;
};

} // namespace qvm
