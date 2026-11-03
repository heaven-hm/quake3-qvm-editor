#pragma once

#include "qvm/common/Error.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace qvm {

struct ProcessResult {
    int exitCode = -1;
    std::string stdOut;
    std::string stdErr;
    bool success = false;
    std::string combinedOutput;
};

using ProcessOutputCallback = std::function<void(std::string_view chunk, bool isStdErr)>;

class ProcessRunner {
public:
    static ProcessResult execute(
        const std::filesystem::path& executable,
        const std::vector<std::string>& arguments,
        const std::filesystem::path& workingDirectory = {},
        ProcessOutputCallback callback = nullptr);
};

} // namespace qvm
