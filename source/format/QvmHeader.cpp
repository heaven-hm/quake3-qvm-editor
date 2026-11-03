#include "qvm/format/QvmHeader.hpp"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace qvm {

static uint32_t readU32LE(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0]) |
          (static_cast<uint32_t>(ptr[1]) << 8) |
          (static_cast<uint32_t>(ptr[2]) << 16) |
          (static_cast<uint32_t>(ptr[3]) << 24);
}

Result<QvmHeader> QvmHeader::parse(std::span<const uint8_t> bytes) {
    if (bytes.size() < kHeaderSizeVer1) {
        return Error(ErrorCode::InvalidHeader, "File size smaller than minimum QVM header size (32 bytes).");
    }

    QvmHeader header;
    header.magic = readU32LE(bytes.data());

    if (!header.isValidMagic()) {
        // Also check if byte swapped
        uint32_t swapped = ((header.magic >> 24) & 0xff) |
                           ((header.magic << 8) & 0xff0000) |
                           ((header.magic >> 8) & 0xff00) |
                           ((header.magic << 24) & 0xff000000);
        if (swapped == kVmMagic || swapped == kVmMagicVer2) {
            return Error(ErrorCode::InvalidMagic, "QVM file is in big-endian byte order (unsupported).");
        }
        std::stringstream ss;
        ss << "Invalid QVM magic: 0x" << std::hex << header.magic << " (expected 0x12721444 or 0x12721445)";
        return Error(ErrorCode::InvalidMagic, ss.str());
    }

    header.instructionCount = readU32LE(bytes.data() + 4);
    header.codeOffset = readU32LE(bytes.data() + 8);
    header.codeLength = readU32LE(bytes.data() + 12);
    header.dataOffset = readU32LE(bytes.data() + 16);
    header.dataLength = readU32LE(bytes.data() + 20);
    header.litLength = readU32LE(bytes.data() + 24);
    header.bssLength = readU32LE(bytes.data() + 28);

    if (header.isVersion2()) {
        if (bytes.size() < kHeaderSizeVer2) {
            return Error(ErrorCode::InvalidHeader, "QVM version 2 header requires 36 bytes.");
        }
        header.jtrgLength = readU32LE(bytes.data() + 32);
    }

    return header;
}

Error QvmHeader::validate(size_t fileSize) const {
    if (!isValidMagic()) {
        return Error(ErrorCode::InvalidMagic, "Invalid QVM magic number.");
    }

    const size_t minHeader = headerSize();
    if (codeOffset < minHeader || codeOffset > fileSize) {
        return Error(ErrorCode::InvalidSectionOffset, "Invalid code section offset.");
    }

    if (static_cast<uint64_t>(codeOffset) + codeLength > fileSize) {
        return Error(ErrorCode::InvalidSectionLength, "Code section extends beyond end of file.");
    }

    if (dataOffset < minHeader || dataOffset > fileSize) {
        return Error(ErrorCode::InvalidSectionOffset, "Invalid data section offset.");
    }

    uint64_t totalData = static_cast<uint64_t>(dataLength) + litLength;
    if (isVersion2()) {
        totalData += jtrgLength;
    }

    if (static_cast<uint64_t>(dataOffset) + totalData > fileSize) {
        return Error(ErrorCode::InvalidSectionLength, "Data/lit section extends beyond end of file.");
    }

    return Error::ok();
}

std::string QvmHeader::formatInfo() const {
    std::stringstream ss;
    ss << "=== QVM Header Information ===\n"
       << "Magic:              0x" << std::hex << std::uppercase << magic
       << (isVersion2() ? " (Version 2 - Jump Table)" : " (Version 1 - Vanilla Q3)") << "\n"
       << std::dec
       << "Instruction Count:  " << instructionCount << " (0x" << std::hex << instructionCount << ")\n"
       << "Code Offset:        0x" << std::hex << codeOffset << " (" << std::dec << codeOffset << " bytes)\n"
       << "Code Length:        0x" << std::hex << codeLength << " (" << std::dec << codeLength << " bytes)\n"
       << "Data Offset:        0x" << std::hex << dataOffset << " (" << std::dec << dataOffset << " bytes)\n"
       << "Data Length:        0x" << std::hex << dataLength << " (" << std::dec << dataLength << " bytes)\n"
       << "Lit Length:         0x" << std::hex << litLength << " (" << std::dec << litLength << " bytes)\n"
       << "BSS Length:         0x" << std::hex << bssLength << " (" << std::dec << bssLength << " bytes)\n";

    if (isVersion2()) {
        ss << "JTRG Length:        0x" << std::hex << jtrgLength << " (" << std::dec << jtrgLength << " bytes)\n";
    }

    return ss.str();
}

} // namespace qvm
