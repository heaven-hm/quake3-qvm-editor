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
    std::filesystem::path outputDir;
    std::string outputName; // e.g. "qagame.qvm", if empty auto-determined by module
    std::filesystem::path sdkDir; // optional path to Quake III SDK root/code
    std::vector<std::filesystem::path> includePaths;
    std::vector<std::string> preprocessorDefines;
    std::filesystem::path toolchainDir; // if empty, auto-detect
    bool debugSymbols = false;
    bool optimize = true;
    bool verbose = false;
    bool cleanIntermediate = false;
};

} // namespace qvm
