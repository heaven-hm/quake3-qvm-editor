#pragma once

#include "qvm/compiler/CompilerConfig.hpp"
#include "qvm/format/QvmInstruction.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvm {

struct FunctionInfo {
    std::string name;
    uint32_t startIndex = 0;
    uint32_t endIndex = 0;
    uint32_t startOffset = 0;
    uint32_t endOffset = 0;
    int32_t stackFrameSize = 0;
    int32_t hash = 0;
    bool isKnown = false;
    std::vector<int32_t> calledAddresses;
    std::vector<std::string> invokedSyscalls;
};

class FunctionAnalyzer {
public:
    static std::vector<FunctionInfo> analyzeFunctions(
        const std::vector<QvmInstruction>& instructions,
        ModuleType moduleType = ModuleType::QaGame);

    static int32_t computeFunctionHash(
        const std::vector<QvmInstruction>& instructions,
        uint32_t startIndex,
        uint32_t endIndex);

    static std::string_view resolveSyscall(int32_t trapNumber, ModuleType moduleType) noexcept;
    static std::string resolveFunctionName(int32_t address, const std::vector<FunctionInfo>& functions);

private:
    static void loadKnownHashes();
};

} // namespace qvm
