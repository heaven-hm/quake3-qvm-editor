#include "qvm/common/FileUtils.hpp"
#include <fstream>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace qvm {

Result<std::vector<uint8_t>> FileUtils::readFileBinary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Error(ErrorCode::FileNotFound, "Unable to open file for reading: " + path.string());
    }

    const auto size = file.tellg();
    if (size < 0) {
        return Error(ErrorCode::FileReadError, "Unable to determine file size: " + path.string());
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return Error(ErrorCode::FileReadError, "Failed to read data from: " + path.string());
    }

    return buffer;
}

Result<std::string> FileUtils::readFileText(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Error(ErrorCode::FileNotFound, "Unable to open file for reading: " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

Error FileUtils::writeFileBinary(const std::filesystem::path& path, std::span<const uint8_t> data) {
    if (auto dirError = ensureDirectory(path.parent_path()); dirError.isError()) {
        return dirError;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Error(ErrorCode::FileWriteError, "Unable to open file for writing: " + path.string());
    }

    if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) {
        return Error(ErrorCode::FileWriteError, "Failed to write data to: " + path.string());
    }

    return Error::ok();
}

Error FileUtils::writeFileText(const std::filesystem::path& path, std::string_view text) {
    if (auto dirError = ensureDirectory(path.parent_path()); dirError.isError()) {
        return dirError;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return Error(ErrorCode::FileWriteError, "Unable to open file for writing: " + path.string());
    }

    file << text;
    if (!file.good()) {
        return Error(ErrorCode::FileWriteError, "Failed to write text to: " + path.string());
    }

    return Error::ok();
}

Error FileUtils::ensureDirectory(const std::filesystem::path& path) {
    if (path.empty()) {
        return Error::ok();
    }
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return Error(ErrorCode::FileWriteError, "Failed to create directory " + path.string() + ": " + ec.message());
    }
    return Error::ok();
}

std::filesystem::path FileUtils::normalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    return ec ? path.lexically_normal() : canonical;
}

std::filesystem::path FileUtils::findExecutable(
    std::string_view name,
    const std::vector<std::filesystem::path>& customSearchPaths) {
    std::vector<std::string> extensions = {""};
#if defined(_WIN32)
    extensions.push_back(".exe");
    extensions.push_back(".cmd");
    extensions.push_back(".bat");
#endif

    auto checkCandidate = [&](const std::filesystem::path& dir, std::string_view baseName) -> std::filesystem::path {
        for (const auto& ext : extensions) {
            std::string candidateName = std::string(baseName) + ext;
            auto candidatePath = dir / candidateName;
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidatePath, ec)) {
                return normalizePath(candidatePath);
            }
        }
        return {};
    };

    // 1. Check custom search paths
    for (const auto& path : customSearchPaths) {
        if (auto found = checkCandidate(path, name); !found.empty()) {
            return found;
        }
    }

    // 2. Check current executable directory
#if defined(_WIN32)
    wchar_t exeBuffer[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exeBuffer, MAX_PATH) > 0) {
        auto exeDir = std::filesystem::path(exeBuffer).parent_path();
        if (auto found = checkCandidate(exeDir, name); !found.empty()) return found;
        if (auto found = checkCandidate(exeDir / "tools", name); !found.empty()) return found;
        if (auto found = checkCandidate(exeDir / "win32", name); !found.empty()) return found;
    }
#elif defined(__APPLE__)
    char exeBuffer[1024];
    uint32_t size = sizeof(exeBuffer);
    if (::_NSGetExecutablePath(exeBuffer, &size) == 0) {
        auto exeDir = std::filesystem::path(exeBuffer).parent_path();
        if (auto found = checkCandidate(exeDir, name); !found.empty()) return found;
        if (auto found = checkCandidate(exeDir / "tools", name); !found.empty()) return found;
    }
#endif

    // 3. Check system PATH
    const char* pathEnv = std::getenv("PATH");
    if (pathEnv) {
        std::string pathStr(pathEnv);
#if defined(_WIN32)
        char delimiter = ';';
#else
        char delimiter = ':';
#endif
        size_t start = 0;
        size_t end = pathStr.find(delimiter);
        while (end != std::string::npos) {
            auto dir = pathStr.substr(start, end - start);
            if (!dir.empty()) {
                if (auto found = checkCandidate(dir, name); !found.empty()) {
                    return found;
                }
            }
            start = end + 1;
            end = pathStr.find(delimiter, start);
        }
        if (start < pathStr.size()) {
            auto dir = pathStr.substr(start);
            if (!dir.empty()) {
                if (auto found = checkCandidate(dir, name); !found.empty()) {
                    return found;
                }
            }
        }
    }

    return {};
}

} // namespace qvm
