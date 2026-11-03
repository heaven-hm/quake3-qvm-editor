#pragma once

#include "qvm/compiler/CompilerConfig.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace qvm {

struct ModuleFileList {
    std::string entryPointFile; // file containing vmMain, MUST be first
    std::vector<std::string> cFiles;
    std::vector<std::string> asmFiles;
    std::vector<std::string> defaultDefines;
    std::vector<std::string> defaultRelativeIncludes;
    std::string defaultOutputBaseName;
};

class ModuleConfig {
public:
    static ModuleFileList getModuleFileList(ModuleType type);
    static std::vector<std::filesystem::path> resolveSourceFiles(
        const std::filesystem::path& baseSourceDir,
        const ModuleFileList& fileList);
};

} // namespace qvm
