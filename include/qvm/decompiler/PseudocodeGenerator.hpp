#pragma once

#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/format/QvmInstruction.hpp"
#include <string>
#include <vector>

namespace qvm {

class PseudocodeGenerator {
public:
    static std::string generate(
        const ParsedQvm& parsedQvm,
        const std::vector<FunctionInfo>& functions,
        ModuleType moduleType = ModuleType::QaGame);

    static std::string generateFunction(
        const ParsedQvm& parsedQvm,
        const FunctionInfo& func,
        const std::unordered_map<uint32_t, std::string>& labels,
        const std::unordered_map<uint32_t, std::string>& stringMap,
        const std::vector<FunctionInfo>& allFunctions,
        ModuleType moduleType);
};

} // namespace qvm
