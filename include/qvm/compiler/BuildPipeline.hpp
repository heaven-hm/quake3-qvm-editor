#pragma once

#include "qvm/common/Error.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/compiler/ToolchainRunner.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace qvm {

struct BuildReport {
    bool success = false;
    std::filesystem::path outputQvmPath;
    size_t qvmFileSize = 0;
    int filesCompiled = 0;
    int totalFiles = 0;
    std::vector<std::string> lccOutputs;
    std::string q3asmOutput;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    double durationSeconds = 0.0;
};

class BuildPipeline {
public:
    static Result<BuildReport> execute(
        const CompilerConfig& config,
        BuildProgressCallback progressCallback = nullptr,
        BuildLogCallback logCallback = nullptr);
};

} // namespace qvm
