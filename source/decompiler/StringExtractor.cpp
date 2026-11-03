#include "qvm/decompiler/StringExtractor.hpp"
#include <cctype>

namespace qvm {

std::vector<ExtractedString> StringExtractor::extractAll(const QvmSections& sections) {
    std::vector<ExtractedString> results;

    // Helper to extract strings from a buffer
    auto scanBuffer = [&](const std::vector<uint8_t>& buf, uint32_t baseOffset, bool isLit, size_t minLength) {
        size_t i = 0;
        while (i < buf.size()) {
            // Find start of printable characters
            if (std::isprint(buf[i]) && buf[i] != '\0') {
                size_t start = i;
                bool valid = true;
                while (i < buf.size() && buf[i] != '\0') {
                    if (!std::isprint(buf[i]) && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
                        valid = false;
                    }
                    ++i;
                }
                // Must be null-terminated
                if (valid && i < buf.size() && buf[i] == '\0') {
                    size_t len = i - start;
                    if (len >= minLength) {
                        std::string text(reinterpret_cast<const char*>(&buf[start]), len);
                        results.push_back(ExtractedString{
                            .offset = baseOffset + static_cast<uint32_t>(start),
                            .text = std::move(text),
                            .inLitSection = isLit
                        });
                    }
                }
            }
            ++i;
        }
    };

    // Lit section is specifically for string constants: minLength 1
    scanBuffer(sections.lit, static_cast<uint32_t>(sections.data.size()), true, 1);

    // Data section contains variables and some strings: minLength 3
    scanBuffer(sections.data, 0, false, 3);

    return results;
}

std::unordered_map<uint32_t, std::string> StringExtractor::buildOffsetMap(
    const std::vector<ExtractedString>& strings) {
    std::unordered_map<uint32_t, std::string> map;
    for (const auto& s : strings) {
        map[s.offset] = s.text;
    }
    return map;
}

} // namespace qvm
