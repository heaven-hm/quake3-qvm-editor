#pragma once

#include "qvm/format/QvmSections.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvm {

struct ExtractedString {
    uint32_t offset = 0;
    std::string text;
    bool inLitSection = true;
};

class StringExtractor {
public:
    static std::vector<ExtractedString> extractAll(const QvmSections& sections);
    static std::unordered_map<uint32_t, std::string> buildOffsetMap(
        const std::vector<ExtractedString>& strings);
};

} // namespace qvm
