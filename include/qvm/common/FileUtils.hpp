#pragma once

#include "qvm/common/Error.hpp"
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace qvm {

class FileUtils {
public:
    static Result<std::vector<uint8_t>> readFileBinary(const std::filesystem::path& path);
    static Result<std::string> readFileText(const std::filesystem::path& path);
    static Error writeFileBinary(const std::filesystem::path& path, std::span<const uint8_t> data);
    static Error writeFileText(const std::filesystem::path& path, std::string_view text);
    static Error ensureDirectory(const std::filesystem::path& path);
    static std::filesystem::path normalizePath(const std::filesystem::path& path);
    static std::filesystem::path getExecutableDirectory();
    static std::filesystem::path findExecutable(
        std::string_view name,
        const std::vector<std::filesystem::path>& customSearchPaths = {});
    static std::vector<std::filesystem::path> resolveQuake3IncludeDirs(
        const std::vector<std::filesystem::path>& userIncludes = {});
};

} // namespace qvm
