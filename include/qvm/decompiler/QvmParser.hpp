#pragma once

#include "qvm/common/Error.hpp"
#include "qvm/format/QvmHeader.hpp"
#include "qvm/format/QvmInstruction.hpp"
#include "qvm/format/QvmSections.hpp"
#include <filesystem>
#include <span>
#include <vector>

namespace qvm {

struct ParsedQvm {
    QvmHeader header;
    QvmSections sections;
    std::vector<QvmInstruction> instructions;
    size_t fileSize = 0;
};

class QvmParser {
public:
    static Result<ParsedQvm> parseFile(const std::filesystem::path& path);
    static Result<ParsedQvm> parseMemory(std::span<const uint8_t> bytes);
};

} // namespace qvm
