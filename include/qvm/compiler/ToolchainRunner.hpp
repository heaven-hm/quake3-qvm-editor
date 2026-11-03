#pragma once

#include "qvm/common/Error.hpp"
#include "qvm/common/ProcessRunner.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace qvm {

struct ToolchainPaths {
    std::filesystem::path lccExecutable;
    std::filesystem::path q3asmExecutable;
    std::filesystem::path q3cppExecutable;
    std::filesystem::path q3rccExecutable;
    bool isValid() const noexcept {
        return !lccExecutable.empty() && !q3asmExecutable.empty();
    }
};

using BuildProgressCallback = std::function<void(int current, int total, std::string_view message)>;
using BuildLogCallback = std::function<void(std::string_view line, bool isError)>;

class ToolchainRunner {
public:
    explicit ToolchainRunner(ToolchainPaths paths);

    static Result<ToolchainPaths> detectToolchain(const std::filesystem::path& hint = {});

    ProcessResult compileCFile(
        const std::filesystem::path& sourceCFile,
        const std::filesystem::path& outputAsmFile,
        const std::vector<std::filesystem::path>& includeDirs,
        const std::vector<std::string>& defines,
        bool debugSymbols,
        BuildLogCallback logCallback = nullptr);

    ProcessResult assembleQvm(
        const std::vector<std::filesystem::path>& asmFiles,
        const std::filesystem::path& outputQvmFile,
        const std::filesystem::path& workingDir,
        bool verbose,
        BuildLogCallback logCallback = nullptr);

    const ToolchainPaths& paths() const noexcept { return paths_; }

private:
    ToolchainPaths paths_;
};

} // namespace qvm
