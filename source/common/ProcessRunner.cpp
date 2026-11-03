#include "qvm/common/ProcessRunner.hpp"
#include <array>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace qvm {

#if defined(_WIN32)

ProcessResult ProcessRunner::execute(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments,
    const std::filesystem::path& workingDirectory,
    ProcessOutputCallback callback) {

    ProcessResult result;
    std::string cmdLine = "\"" + executable.string() + "\"";
    for (const auto& arg : arguments) {
        cmdLine += " \"";
        for (char c : arg) {
            if (c == '"') cmdLine += "\\\"";
            else cmdLine += c;
        }
        cmdLine += "\"";
    }
    result.commandLine = cmdLine;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hChildStdOutRead = nullptr;
    HANDLE hChildStdOutWrite = nullptr;
    if (!CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &saAttr, 0)) {
        result.stdErr = "Failed to create stdout pipe.";
        return result;
    }
    SetHandleInformation(hChildStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStdOutWrite;
    siStartInfo.hStdOutput = hChildStdOutWrite;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    const char* workDirPtr = workingDirectory.empty() ? nullptr : workingDirectory.string().c_str();

    std::vector<char> cmdLineMutable(cmdLine.begin(), cmdLine.end());
    cmdLineMutable.push_back('\0');

    BOOL bSuccess = CreateProcessA(
        nullptr,
        cmdLineMutable.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDirPtr,
        &siStartInfo,
        &piProcInfo);

    CloseHandle(hChildStdOutWrite);

    if (!bSuccess) {
        CloseHandle(hChildStdOutRead);
        result.stdErr = "Failed to launch process: " + executable.string();
        return result;
    }

    DWORD dwRead;
    CHAR chBuf[4096];
    std::string lineAccumulator;

    for (;;) {
        bSuccess = ReadFile(hChildStdOutRead, chBuf, sizeof(chBuf) - 1, &dwRead, nullptr);
        if (!bSuccess || dwRead == 0) break;
        chBuf[dwRead] = '\0';
        result.stdOut.append(chBuf, dwRead);
        result.combinedOutput.append(chBuf, dwRead);
        if (callback) {
            callback(std::string_view(chBuf, dwRead), false);
        }
    }

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(piProcInfo.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.success = (result.exitCode == 0);

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hChildStdOutRead);

    return result;
}

#else

ProcessResult ProcessRunner::execute(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments,
    const std::filesystem::path& workingDirectory,
    ProcessOutputCallback callback) {

    ProcessResult result;
    std::string cmdLine = "\"" + executable.string() + "\"";
    for (const auto& arg : arguments) {
        if (arg.find(' ') != std::string::npos || arg.find('"') != std::string::npos) {
            cmdLine += " \"";
            for (char c : arg) {
                if (c == '"') cmdLine += "\\\"";
                else cmdLine += c;
            }
            cmdLine += "\"";
        } else {
            cmdLine += " " + arg;
        }
    }
    result.commandLine = cmdLine;

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        result.stdErr = "Failed to create pipe.";
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        result.stdErr = "Failed to fork process.";
        return result;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        if (!workingDirectory.empty()) {
            if (chdir(workingDirectory.c_str()) != 0) {
                _exit(127);
            }
        }

        std::vector<const char*> argv;
        std::string exeStr = executable.string();
        argv.push_back(exeStr.c_str());
        for (const auto& arg : arguments) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        _exit(127);
    } else {
        // Parent process
        close(pipefd[1]);

        char buffer[4096];
        ssize_t bytesRead = 0;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            result.stdOut.append(buffer, bytesRead);
            result.combinedOutput.append(buffer, bytesRead);
            if (callback) {
                callback(std::string_view(buffer, bytesRead), false);
            }
        }
        close(pipefd[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            result.exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.exitCode = 128 + WTERMSIG(status);
        } else {
            result.exitCode = -1;
        }
        result.success = (result.exitCode == 0);
    }

    return result;
}

#endif

} // namespace qvm
