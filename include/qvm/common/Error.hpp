#pragma once

#include <string>
#include <system_error>
#include <variant>

namespace qvm {

enum class ErrorCode {
    Success = 0,
    FileNotFound,
    FileReadError,
    FileWriteError,
    InvalidMagic,
    InvalidHeader,
    InvalidSectionOffset,
    InvalidSectionLength,
    InvalidInstructionCount,
    UnknownOpcode,
    TruncatedInstruction,
    CompilerNotFound,
    AssemblerNotFound,
    CompilationFailed,
    AssemblyFailed,
    ProcessExecutionFailed,
    InvalidModule,
    InvalidProjectDirectory,
    DecompilationFailed,
    // QVM runtime execution errors (added for the embedded q3vm interpreter).
    RuntimeError,
    RuntimeFileNotFound,
    RuntimeInvalidImage,
    RuntimeExecutionFailed
};

class Error {
public:
    Error() : code_(ErrorCode::Success) {}
    Error(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

    [[nodiscard]] bool isOk() const noexcept { return code_ == ErrorCode::Success; }
    [[nodiscard]] bool isError() const noexcept { return !isOk(); }
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

    static Error ok() { return Error(); }

private:
    ErrorCode code_;
    std::string message_;
};

template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}
    Result(ErrorCode code, std::string message) : data_(Error(code, std::move(message))) {}

    [[nodiscard]] bool hasValue() const noexcept { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool hasError() const noexcept { return std::holds_alternative<Error>(data_); }

    [[nodiscard]] const T& value() const { return std::get<T>(data_); }
    [[nodiscard]] T& value() { return std::get<T>(data_); }

    [[nodiscard]] const Error& error() const { return std::get<Error>(data_); }

    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
    [[nodiscard]] const T& operator*() const { return value(); }
    [[nodiscard]] T& operator*() { return value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }
    [[nodiscard]] T* operator->() { return &value(); }

private:
    std::variant<T, Error> data_;
};

} // namespace qvm
