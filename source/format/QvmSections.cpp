#include "qvm/format/QvmSections.hpp"
#include <cstring>

namespace qvm {

const uint8_t* QvmSections::dataAt(uint32_t offset) const noexcept {
    if (offset < data.size()) {
        return data.data() + offset;
    }
    const uint32_t litOffset = offset - static_cast<uint32_t>(data.size());
    if (litOffset < lit.size()) {
        return lit.data() + litOffset;
    }
    return nullptr;
}

std::string_view QvmSections::stringAt(uint32_t offset) const noexcept {
    const uint8_t* ptr = dataAt(offset);
    if (!ptr) {
        return {};
    }

    size_t maxLen = 0;
    if (offset < data.size()) {
        maxLen = data.size() - offset;
    } else {
        maxLen = lit.size() - (offset - data.size());
    }

    size_t len = 0;
    while (len < maxLen && ptr[len] != '\0') {
        ++len;
    }

    return std::string_view(reinterpret_cast<const char*>(ptr), len);
}

} // namespace qvm
