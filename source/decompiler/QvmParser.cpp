#include "qvm/decompiler/QvmParser.hpp"
#include "qvm/common/FileUtils.hpp"
#include "qvm/decompiler/InstructionDecoder.hpp"

namespace qvm {

Result<ParsedQvm> QvmParser::parseFile(const std::filesystem::path& path) {
    auto fileBytesRes = FileUtils::readFileBinary(path);
    if (fileBytesRes.hasError()) {
        return fileBytesRes.error();
    }

    auto parsedRes = parseMemory(fileBytesRes.value());
    if (parsedRes.hasError()) {
        return parsedRes.error();
    }

    auto parsed = std::move(parsedRes.value());
    parsed.fileSize = fileBytesRes.value().size();
    return parsed;
}

Result<ParsedQvm> QvmParser::parseMemory(std::span<const uint8_t> bytes) {
    auto headerRes = QvmHeader::parse(bytes);
    if (headerRes.hasError()) {
        return headerRes.error();
    }

    const QvmHeader& header = headerRes.value();
    if (auto err = header.validate(bytes.size()); err.isError()) {
        return err;
    }

    ParsedQvm result;
    result.header = header;
    result.fileSize = bytes.size();

    // Extract CODE section
    auto codeSpan = bytes.subspan(header.codeOffset, header.codeLength);
    result.sections.code.assign(codeSpan.begin(), codeSpan.end());

    // Extract DATA section
    auto dataSpan = bytes.subspan(header.dataOffset, header.dataLength);
    result.sections.data.assign(dataSpan.begin(), dataSpan.end());

    // Extract LIT section
    auto litSpan = bytes.subspan(header.dataOffset + header.dataLength, header.litLength);
    result.sections.lit.assign(litSpan.begin(), litSpan.end());

    // Extract JTRG section if Version 2
    if (header.isVersion2() && header.jtrgLength > 0) {
        auto jtrgSpan = bytes.subspan(header.dataOffset + header.dataLength + header.litLength, header.jtrgLength);
        result.sections.jumpTable.assign(jtrgSpan.begin(), jtrgSpan.end());
    }

    // Decode instructions
    auto decodeRes = InstructionDecoder::decode(result.sections.code, header.instructionCount);
    if (decodeRes.hasError()) {
        return decodeRes.error();
    }

    result.instructions = std::move(decodeRes.value());
    return result;
}

} // namespace qvm
