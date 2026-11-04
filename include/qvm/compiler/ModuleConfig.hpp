#pragma once

#include "qvm/compiler/CompilerConfig.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace qvm {

struct ModuleFileList {
    // Canonical child of a Quake III code root that owns the module's entry
    // sources.  This disambiguates SDKs that also ship unrelated top-level
    // files with names such as g_main.c.
    std::string primarySourceDirectory;
    std::string entryPointFile; // file containing vmMain, MUST be first
    std::vector<std::string> cFiles;
    std::vector<std::string> asmFiles;
    std::vector<std::string> defaultDefines;
    std::vector<std::string> defaultRelativeIncludes;
    std::string defaultOutputBaseName;
};

struct ModuleMapping {
    std::string sourceFile;       // "mod.c"
    std::string syscallFile;      // "cg_syscalls.asm", "g_syscalls.asm", "ui_syscalls.asm"
    std::string outputFile;       // "cgame.qvm", "qagame.qvm", "ui.qvm"
    std::vector<std::string> defaultDefines; // e.g. {"-DQ3_VM", "-DCGAME"}
};

class ModuleConfig {
public:
    static ModuleFileList getModuleFileList(ModuleType type);
    static ModuleMapping getModuleMapping(ModuleType type);
    static ModuleType deduceModuleFromOutputName(std::string_view outputName);
    static ModuleType deduceModuleFromSourceOrSyscall(std::string_view filename);
    static std::vector<std::filesystem::path> resolveSourceFiles(
        const std::filesystem::path& baseSourceDir,
        const ModuleFileList& fileList);
};

} // namespace qvm
