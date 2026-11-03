#pragma once

#include "qvm/format/QvmHeader.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace qvm {

struct QvmSections {
    QvmHeader header;
    std::vector<uint8_t> code;
    std::vector<uint8_t> data;
    std::vector<uint8_t> lit;
    std::vector<uint8_t> jumpTable;

    [[nodiscard]] size_t totalDataSize() const noexcept {
        return data.size() + lit.size();
    }

    [[nodiscard]] const uint8_t* dataAt(uint32_t offset) const noexcept;
    [[nodiscard]] std::string_view stringAt(uint32_t offset) const noexcept;
};

} // namespace qvm
