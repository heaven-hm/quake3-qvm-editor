#pragma once

#include "qvm/common/Error.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include <filesystem>
#include <string>

namespace qvm {

class Exporter {
public:
    static std::string generateDisassembly(
        const ParsedQvm& parsedQvm,
        ModuleType moduleType = ModuleType::QaGame);

    static std::string generateFunctionDisassembly(
        const ParsedQvm& parsedQvm,
        const FunctionInfo& func,
        ModuleType moduleType = ModuleType::QaGame);

    static Error exportDisassembly(
        const ParsedQvm& parsedQvm,
        const std::filesystem::path& outputPath,
        ModuleType moduleType = ModuleType::QaGame);

    static Error exportPseudocode(
        const ParsedQvm& parsedQvm,
        const std::filesystem::path& outputPath,
        ModuleType moduleType = ModuleType::QaGame);

    static Error exportBoth(
        const ParsedQvm& parsedQvm,
        const std::filesystem::path& asmOutputPath,
        const std::filesystem::path& pseudocodeOutputPath,
        ModuleType moduleType = ModuleType::QaGame);
};

} // namespace qvm
