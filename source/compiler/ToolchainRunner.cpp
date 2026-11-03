#include "qvm/compiler/ToolchainRunner.hpp"
#include "qvm/common/FileUtils.hpp"
#include <iostream>

namespace qvm {

ToolchainRunner::ToolchainRunner(ToolchainPaths paths) : paths_(std::move(paths)) {}

Result<ToolchainPaths> ToolchainRunner::detectToolchain(const std::filesystem::path& hint) {
    ToolchainPaths paths;
    std::vector<std::filesystem::path> searchDirs;

    if (!hint.empty()) {
        searchDirs.push_back(hint);
        searchDirs.push_back(hint / "tools");
        searchDirs.push_back(hint / "bin");
        searchDirs.push_back(hint / "bin" / "win32");
        searchDirs.push_back(hint / "bin" / "linux");
    }

    // Common build directories and bundle locations
    searchDirs.push_back(std::filesystem::current_path());
    searchDirs.push_back(std::filesystem::current_path() / "tools");
    searchDirs.push_back(std::filesystem::current_path() / "bin");
    searchDirs.push_back(std::filesystem::current_path() / "third_party" / "quake3" / "bin" / "win32");

    // Look for lcc (prefer q3lcc if available)
    paths.lccExecutable = FileUtils::findExecutable("q3lcc", searchDirs);
    if (paths.lccExecutable.empty()) {
        paths.lccExecutable = FileUtils::findExecutable("lcc", searchDirs);
    }

    // Look for q3asm
    paths.q3asmExecutable = FileUtils::findExecutable("q3asm", searchDirs);

    // Look for helper tools (optional if lcc driver resolves them)
    paths.q3cppExecutable = FileUtils::findExecutable("q3cpp", searchDirs);
    paths.q3rccExecutable = FileUtils::findExecutable("q3rcc", searchDirs);

    if (!paths.isValid()) {
        std::string missing;
        if (paths.lccExecutable.empty()) missing += " LCC compiler (q3lcc/lcc)";
        if (paths.q3asmExecutable.empty()) {
            if (!missing.empty()) missing += " and";
            missing += " Q3ASM assembler (q3asm)";
        }
        return Error(ErrorCode::CompilerNotFound,
                     "Quake III toolchain not found:" + missing +
                     ". Please ensure the tools directory is built or specified.");
    }

    return paths;
}

ProcessResult ToolchainRunner::compileCFile(
    const std::filesystem::path& sourceCFile,
    const std::filesystem::path& outputAsmFile,
    const std::vector<std::filesystem::path>& includeDirs,
    const std::vector<std::string>& defines,
    bool debugSymbols,
    BuildLogCallback logCallback) {

    std::vector<std::string> args;
    args.push_back("-S");
    args.push_back("-Wf-target=bytecode");

    if (debugSymbols) {
        args.push_back("-Wf-g");
    }

    for (const auto& def : defines) {
        if (!def.empty()) {
            if (def.starts_with("-D")) {
                args.push_back(def);
            } else {
                args.push_back("-D" + def);
            }
        }
    }

    for (const auto& inc : includeDirs) {
        if (!inc.empty()) {
            args.push_back("-I" + std::filesystem::absolute(inc).string());
        }
    }

    auto absOutAsm = std::filesystem::absolute(outputAsmFile);
    auto absSrcC = std::filesystem::absolute(sourceCFile);

    args.push_back("-o");
    args.push_back(absOutAsm.string());
    args.push_back(absSrcC.string());

    auto callback = [&](std::string_view chunk, bool isStdErr) {
        if (logCallback) {
            logCallback(chunk, isStdErr);
        }
    };

    return ProcessRunner::execute(
        paths_.lccExecutable,
        args,
        absOutAsm.parent_path(),
        callback);
}

ProcessResult ToolchainRunner::assembleQvm(
    const std::vector<std::filesystem::path>& asmFiles,
    const std::filesystem::path& outputQvmFile,
    const std::filesystem::path& workingDir,
    bool verbose,
    BuildLogCallback logCallback) {

    std::vector<std::string> args;

    // q3asm takes -o <base_name_without_extension>
    std::string baseName = outputQvmFile.stem().string();
    args.push_back("-o");
    args.push_back(baseName);

    if (verbose) {
        args.push_back("-v");
    }

    for (const auto& asmFile : asmFiles) {
        args.push_back(std::filesystem::absolute(asmFile).string());
    }

    std::string cmdLine = "\"" + paths_.q3asmExecutable.string() + "\"";
    for (const auto& arg : args) {
        if (arg.find(' ') != std::string::npos || arg.find('"') != std::string::npos) {
            cmdLine += " \"" + arg + "\"";
        } else {
            cmdLine += " " + arg;
        }
    }

    if (logCallback) {
        logCallback("[Q3ASM Command]: " + cmdLine, false);
    }

    auto callback = [&](std::string_view chunk, bool isStdErr) {
        if (logCallback) {
            logCallback(chunk, isStdErr);
        }
    };

    auto res = ProcessRunner::execute(
        paths_.q3asmExecutable,
        args,
        std::filesystem::absolute(workingDir),
        callback);

    res.commandLine = cmdLine;
    return res;
}

} // namespace qvm
