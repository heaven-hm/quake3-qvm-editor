#pragma once

#include "qvm/common/Error.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace qvm {

/// Syscall table the loaded module was linked against. Quake III uses a
/// different import numbering for qagame, cgame and ui, so the runtime has to
/// know which one the bytecode expects.
enum class RuntimeModule {
    QaGame,
    Cgame,
    Ui,
    Custom // custom mods link g_syscalls.asm, i.e. the qagame numbering
};

struct RuntimeOptions {
    RuntimeModule module = RuntimeModule::QaGame;
    /// Passed to vmMain() as `command` (Q3: 0 == GAME_INIT / CG_INIT / UI_INIT).
    int command = 0;
    /// Extra vmMain() arguments (arg0..argN), appended after `command`.
    std::vector<int> args;
    /// Record every system trap the bytecode performs.
    bool traceSyscalls = false;
};

struct RuntimeResult {
    bool success = false;
    int64_t returnValue = 0;          ///< value returned by vmMain()
    std::string error;                ///< trap_Error text / interpreter failure
    std::vector<std::string> output;  ///< everything the bytecode printed
    std::vector<std::string> trace;   ///< syscall trace (RuntimeOptions::traceSyscalls)
    int syscallCount = 0;
    double durationMs = 0.0;
};

/// Runs compiled .qvm bytecode with the embedded q3vm interpreter
/// (third_party/q3vm) and a Quake III syscall shim.
class QvmRuntime {
public:
    /// Called for every line the bytecode prints; `isError` is true for trap_Error.
    using OutputCallback = std::function<void(std::string_view line, bool isError)>;

    static Result<RuntimeResult> run(const std::filesystem::path& qvmPath,
                                     const RuntimeOptions& options = {},
                                     OutputCallback onOutput = nullptr);
};

[[nodiscard]] const char* runtimeModuleName(RuntimeModule module) noexcept;
[[nodiscard]] RuntimeModule runtimeModuleFromString(std::string_view name);

} // namespace qvm
