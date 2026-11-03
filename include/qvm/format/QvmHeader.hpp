#pragma once

#include "qvm/common/Error.hpp"
#include <cstdint>
#include <span>
#include <string>

namespace qvm {

inline constexpr uint32_t kVmMagic = 0x12721444;
inline constexpr uint32_t kVmMagicVer2 = 0x12721445;
inline constexpr size_t kHeaderSizeVer1 = 32;
inline constexpr size_t kHeaderSizeVer2 = 36;

struct QvmHeader {
    uint32_t magic = 0;
    uint32_t instructionCount = 0;
    uint32_t codeOffset = 0;
    uint32_t codeLength = 0;
    uint32_t dataOffset = 0;
    uint32_t dataLength = 0;
    uint32_t litLength = 0;
    uint32_t bssLength = 0;
    uint32_t jtrgLength = 0; // version 2 only

    [[nodiscard]] bool isVersion2() const noexcept { return magic == kVmMagicVer2; }
    [[nodiscard]] bool isValidMagic() const noexcept {
        return magic == kVmMagic || magic == kVmMagicVer2;
    }
    [[nodiscard]] size_t headerSize() const noexcept {
        return isVersion2() ? kHeaderSizeVer2 : kHeaderSizeVer1;
    }

    [[nodiscard]] Error validate(size_t fileSize) const;
    [[nodiscard]] std::string formatInfo() const;

    static Result<QvmHeader> parse(std::span<const uint8_t> bytes);
};

} // namespace qvm
