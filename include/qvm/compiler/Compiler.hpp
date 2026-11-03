#pragma once

#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/compiler/CompilerConfig.hpp"

namespace qvm {

class Compiler {
public:
    static Result<BuildReport> compile(
        const CompilerConfig& config,
        BuildProgressCallback progressCallback = nullptr,
        BuildLogCallback logCallback = nullptr) {
        return BuildPipeline::execute(config, progressCallback, logCallback);
    }
};

} // namespace qvm
