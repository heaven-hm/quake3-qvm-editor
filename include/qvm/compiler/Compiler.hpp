#pragma once

#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/compiler/CompilerConfig.hpp"

namespace qvm {

class Compiler {
public:
    /// Compile Quake 3 C source (directory or single file) into a QVM module.
    static Result<BuildReport> compile(
        const CompilerConfig& config,
        BuildProgressCallback progressCallback = nullptr,
        BuildLogCallback logCallback = nullptr) {
        return BuildPipeline::execute(config, progressCallback, logCallback);
    }

    /// Compile an in-memory C source string into a full QVM module.
    /// The caller's buffer is written to a temporary mod.c and linked against
    /// the matching module tree when a module root can be located automatically.
    static Result<BuildReport> compileSourceCode(
        std::string_view cCode,
        CompilerConfig config = {},
        BuildProgressCallback progressCallback = nullptr,
        BuildLogCallback logCallback = nullptr);
};

} // namespace qvm
