#include "qvm/common/FileUtils.hpp"
#include <algorithm>
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
        if (auto found = checkCandidate(exeDir.parent_path() / "Resources" / "tools", name); !found.empty()) return found;
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

std::filesystem::path FileUtils::getExecutableDirectory() {
#if defined(_WIN32)
    wchar_t exeBuffer[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exeBuffer, MAX_PATH) > 0) {
        return std::filesystem::path(exeBuffer).parent_path();
    }
#elif defined(__APPLE__)
    char exeBuffer[1024];
    uint32_t size = sizeof(exeBuffer);
    if (::_NSGetExecutablePath(exeBuffer, &size) == 0) {
        return std::filesystem::path(exeBuffer).parent_path();
    }
#else
    char exeBuffer[1024];
    ssize_t len = ::readlink("/proc/self/exe", exeBuffer, sizeof(exeBuffer) - 1);
    if (len > 0) {
        exeBuffer[len] = '\0';
        return std::filesystem::path(exeBuffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::vector<std::filesystem::path> FileUtils::resolveQuake3IncludeDirs(
    const std::vector<std::filesystem::path>& userIncludes,
    const std::filesystem::path& sdkDir) {

    std::vector<std::filesystem::path> results;
    auto addUnique = [&results](const std::filesystem::path& p) {
        if (p.empty()) return;
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && std::filesystem::is_directory(p, ec)) {
            auto canonical = std::filesystem::weakly_canonical(p, ec);
            if (std::find(results.begin(), results.end(), canonical) == results.end() &&
                std::find(results.begin(), results.end(), p) == results.end()) {
                results.push_back(p);
            }
        }
    };

    auto expandSdkModules = [&addUnique](const std::filesystem::path& root) {
        if (root.empty()) return;
        addUnique(root);
        for (const auto& sub : {"qcommon", "game", "cgame", "ui", "q3_ui", "client", "server"}) {
            addUnique(root / sub);
            addUnique(root / "code" / sub);
            addUnique(root / "build" / "quake3" / "code" / sub);
        }
    };

    // 1. Process explicit SDK directory first if provided
    if (!sdkDir.empty()) {
        expandSdkModules(sdkDir);
        expandSdkModules(sdkDir.parent_path());
        expandSdkModules(sdkDir.parent_path().parent_path());
    }

    // 2. Process all user-specified include paths and expand sibling SDK modules
    for (auto inc : userIncludes) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(inc, ec)) {
            inc = inc.parent_path();
        }
        expandSdkModules(inc);
        expandSdkModules(inc.parent_path());
        expandSdkModules(inc.parent_path().parent_path());
    }

    // 2. Discover Quake 3 headers relative to executable / application bundle
    auto exeDir = getExecutableDirectory();
    std::vector<std::filesystem::path> baseDirs = {
        exeDir,
        exeDir / "tools",
        exeDir.parent_path() / "Resources",
        exeDir.parent_path().parent_path().parent_path(),
        std::filesystem::current_path()
    };

    for (const auto& base : baseDirs) {
        for (const auto& mod : {"game", "cgame", "qcommon", "ui", "q3_ui"}) {
            addUnique(base / "code" / mod);
            addUnique(base / "third_party" / "quake3" / "code" / mod);
        }
    }

    // 3. Fallback well-known local paths
    for (const auto& wellKnown : {
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/build/quake3/code/qcommon",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/build/quake3/code/cgame",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/build/quake3/code/game",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/build/quake3/code/ui",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/code/qcommon",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/code/cgame",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/code/game",
        "/Users/haseeb-mir/Documents/Code/quake-3-macos/code/ui"
    }) {
        addUnique(wellKnown);
    }

    return results;
}

} // namespace qvm
