#include "qvm/runtime/QvmRuntime.hpp"

#include "qvm/format/QvmHeader.hpp"
#include "vm.h"
#include <algorithm>
#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <iterator>
#include <cctype>

namespace qvm {

[[nodiscard]] const char* runtimeModuleName(RuntimeModule module) noexcept {
    switch (module) {
    case RuntimeModule::QaGame: return "qagame";
    case RuntimeModule::Cgame:  return "cgame";
    case RuntimeModule::Ui:     return "ui";
    case RuntimeModule::Custom: return "custom";
    }
    return "unknown";
}

[[nodiscard]] RuntimeModule runtimeModuleFromString(std::string_view name) {
    std::string norm(name);
    for (auto& c : norm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (norm == "qagame" || norm == "game" || norm == "g")
        return RuntimeModule::QaGame;
    if (norm == "cgame" || norm == "cg" || norm == "clientgame")
        return RuntimeModule::Cgame;
    if (norm == "ui" || norm == "q3_ui" || norm == "u")
        return RuntimeModule::Ui;
    if (norm == "custom" || norm == "cust")
        return RuntimeModule::Custom;
    return RuntimeModule::QaGame;
}

// ---------------------------------------------------------------------------
// Host state + callbacks (C linkage, required by third_party/q3vm/vm.c)
// ---------------------------------------------------------------------------

struct HostState {
    std::jmp_buf errorJump;     // plain C jmp_buf (from <csetjmp>)
    bool jumpReady = false;
    std::string lastError;
    uint32_t errorCode = 0;
    std::vector<void*> mallocedBlocks;
    RuntimeOptions options;
    std::vector<std::string> output;
    std::vector<std::string> trace;
    int syscallCount = 0;
    std::function<void(std::string_view, bool)> outputCb;
    std::string pendingLine;
    std::string pendingError;
    std::unordered_map<int, std::string> configStrings;
    std::vector<std::string> entityTokens{"{", "classname", "worldspawn", "}"};
    size_t nextEntityToken = 0;
    std::chrono::steady_clock::time_point runStart;
};

static thread_local HostState* g_host = nullptr;

extern "C" {

void* Com_malloc(size_t size, vm_t*, vmMallocType_t) {
    void* p = std::malloc(size);
    if (p && g_host) g_host->mallocedBlocks.push_back(p);
    return p;
}

void Com_free(void* p, vm_t*, vmMallocType_t) {
    if (!p) return;
    if (g_host) {
        auto& v = g_host->mallocedBlocks;
        const auto it = std::find(v.begin(), v.end(), p);
        if (it != v.end()) v.erase(it);
    }
    std::free(p);
}

void Com_Error(vmErrorCode_t level, const char* error) {
    if (!g_host) {
        std::fprintf(stderr, "q3vm: error %u: %s\n", (unsigned)level,
                     error ? error : "(null)");
        std::abort();
    }
    g_host->errorCode = static_cast<uint32_t>(level);
    g_host->lastError = error ? error : "...";
    // trim CR/LF
    g_host->lastError.erase(
        std::remove(g_host->lastError.begin(), g_host->lastError.end(), '\n'),
        std::remove(g_host->lastError.begin(), g_host->lastError.end(), '\r'));
    if (g_host->jumpReady) std::longjmp(g_host->errorJump, 1);
    std::abort();
}

} // extern "C"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string readVmString(vm_t* vm, intptr_t vmAddr) {
    // Address zero is a valid QVM data-segment address (and frequently holds
    // the first string literal), so only the VM/data pointers themselves are
    // invalid here.
    if (!vm || !vm->dataBase) return {};
    // The QVM interpreter deliberately masks every VM address, allowing its
    // virtual address space to wrap at dataMask.  Mirror that behavior rather
    // than rejecting valid addresses above the physical allocation size.
    const uint32_t dest = static_cast<uint32_t>(vmAddr) & static_cast<uint32_t>(vm->dataMask);
    const size_t avail = static_cast<size_t>(vm->dataAlloc) - dest;
    if (avail == 0) return {};
    const char* bytes = reinterpret_cast<const char*>(vm->dataBase) + dest;
    size_t n = 0;
    while (n < avail && n < 4096 && bytes[n] != '\0') ++n;
    return std::string(bytes, n);
}

static void copyVmBuffer(vm_t* vm, intptr_t vmAddr, size_t len, const char* src) {
    if (!vm || !vm->dataBase || !src || len == 0) return;
    const uint32_t dest = static_cast<uint32_t>(vmAddr) & static_cast<uint32_t>(vm->dataMask);
    if (dest + len > static_cast<uint32_t>(vm->dataAlloc)) return;
    char* dst = reinterpret_cast<char*>(vm->dataBase) + dest;
    std::memcpy(dst, src, std::min(len, std::strlen(src) + 1));
}

static void writeVmInt(vm_t* vm, intptr_t vmAddr, int32_t value) {
    if (!vm || !vm->dataBase) return;
    const size_t dest = static_cast<uint32_t>(vmAddr) & static_cast<uint32_t>(vm->dataMask);
    if (dest + sizeof(value) > static_cast<size_t>(vm->dataAlloc)) return;
    std::memcpy(vm->dataBase + dest, &value, sizeof(value));
}

static void flushLine(HostState* host, std::string_view text, bool isError) {
    if (host->outputCb) host->outputCb(text, isError);
}

// Syscall numbering mirrors the Qt3 module public headers used by this project.
//
//   qagame:  g_public.h  gameImport_t    (G_PRINT=0, G_ERROR=1, ...)
//   cgame:   cg_public.h cgImport_t       (CG_PRINT=0, CG_ERROR=1, ...)
//   ui:      ui_public.h uiImport_t       (UI_ERROR=0, UI_PRINT=1, UI_MILLISECONDS=2, ...)
//
// Source of truth for any future addition: Id Software Q3A source in
// third_party/quake3/code/game/g_public.h, cgame/cg_public.h, ui/ui_public.h.

static std::string_view trapName(RuntimeModule module, int number) {
    // These QVM ABI imports have the same negative syscall values for every
    // module; handle them before the module-specific engine import table.
    switch (number) {
    case 100: return "memset";
    case 101: return "memcpy";
    case 102: return "strncpy";
    default: break;
    }
    switch (module) {
    case RuntimeModule::QaGame:
        switch (number) {
        case 0:  return "G_PRINT";
        case 1:  return "G_ERROR";
        case 2:  return "G_MILLISECONDS";
        case 3:  return "G_CVAR_REGISTER";
        case 4:  return "G_CVAR_UPDATE";
        case 5:  return "G_CVAR_SET";
        case 6:  return "G_CVAR_VARIABLE_INTEGER_VALUE";
        case 7:  return "G_CVAR_VARIABLE_STRING_BUFFER";
        case 8:  return "G_ARGC";
        case 9:  return "G_ARGV";
        case 10: return "G_FS_FOPEN_FILE";
        case 11: return "G_FS_READ";
        case 12: return "G_FS_WRITE";
        case 13: return "G_FS_FCLOSE_FILE";
        case 14: return "G_SEND_CONSOLE_COMMAND";
        case 15: return "G_LOCATE_GAME_DATA";
        case 16: return "G_DROP_CLIENT";
        case 17: return "G_SEND_SERVER_COMMAND";
        case 18: return "G_SET_CONFIGSTRING";
        case 19: return "G_GET_CONFIGSTRING";
        case 20: return "G_GET_USERINFO";
        case 21: return "G_SET_USERINFO";
        case 22: return "G_GET_SERVERINFO";
        case 23: return "G_SET_BRUSH_MODEL";
        case 24: return "G_TRACE";
        case 25: return "G_POINT_CONTENTS";
        case 37: return "G_GET_ENTITY_TOKEN";
        default: return "G_trap";
        }
    case RuntimeModule::Cgame:
        switch (number) {
        case 0:  return "CG_PRINT";
        case 1:  return "CG_ERROR";
        case 2:  return "CG_MILLISECONDS";
        case 3:  return "CG_CVAR_REGISTER";
        case 4:  return "CG_CVAR_UPDATE";
        case 5:  return "CG_CVAR_SET";
        case 6:  return "CG_CVAR_VARIABLESTRINGBUFFER";
        case 7:  return "CG_ARGC";
        case 8:  return "CG_ARGV";
        case 9:  return "CG_ARGS";
        case 10: return "CG_FS_FOPENFILE";
        case 11: return "CG_FS_READ";
        case 12: return "CG_FS_WRITE";
        case 13: return "CG_FS_FCLOSEFILE";
        case 14: return "CG_SENDCONSOLECOMMAND";
        case 15: return "CG_ADDCOMMAND";
        case 16: return "CG_SENDCLIENTCOMMAND";
        case 17: return "CG_UPDATESCREEN";
        case 18: return "CG_CM_LOADMAP";
        case 19: return "CG_CM_NUMINLINEMODELS";
        case 49: return "CG_GETGLCONFIG";
        case 50: return "CG_GETGAMESTATE";
        default: return "CG_trap";
        }
    case RuntimeModule::Ui:
        switch (number) {
        case 0:  return "UI_ERROR";
        case 1:  return "UI_PRINT";
        case 2:  return "UI_MILLISECONDS";
        case 3:  return "UI_CVAR_SET";
        case 4:  return "UI_CVAR_VARIABLEVALUE";
        case 5:  return "UI_CVAR_VARIABLESTRINGBUFFER";
        case 6:  return "UI_CVAR_SETVALUE";
        case 7:  return "UI_CVAR_RESET";
        case 8:  return "UI_CVAR_CREATE";
        case 9:  return "UI_CVAR_INFOSTRINGBUFFER";
        case 10: return "UI_ARGC";
        case 11: return "UI_ARGV";
        case 12: return "UI_CMD_EXECUTETEXT";
        case 13: return "UI_FS_FOPENFILE";
        case 14: return "UI_FS_READ";
        case 15: return "UI_FS_WRITE";
        case 16: return "UI_FS_FCLOSEFILE";
        case 17: return "UI_FS_GETFILELIST";
        case 18: return "UI_R_REGISTERMODEL";
        case 19: return "UI_R_REGISTERSKIN";
        default: return "UI_trap";
        }
    case RuntimeModule::Custom:
        // Custom mods are linked against g_syscalls.asm -> qagame numbering.
        return trapName(RuntimeModule::QaGame, number);
    }
    return "trap";
}

extern "C" intptr_t qvmSystemCalls(vm_t* vm, intptr_t* args) {
    HostState* host = g_host;
    if (!host) Com_Error(VM_NO_SYSCALL_CALLBACK, "no syscall host");

    ++host->syscallCount;
    const int number = static_cast<int>(args[0]);
    auto name = trapName(host->options.module, number);

    if (host->options.traceSyscalls) {
        host->trace.push_back(std::to_string(number) + " (" + std::string(name) +
                              ") -> 0");
    }

    // ----- text / error traps -------------------------------------------------
    if (name == "G_PRINT" || name == "CG_PRINT" || name == "UI_PRINT") {
        std::string s = readVmString(vm, args[1]);
        host->pendingLine += s;
        if (!host->pendingLine.empty() && host->pendingLine.back() == '\n') {
            std::string line = std::move(host->pendingLine);
            host->pendingLine.clear();
            flushLine(host, std::string_view(line), false);
            host->output.push_back(std::move(line));
        }
        return args[1];
    }

    if (name == "G_ERROR" || name == "CG_ERROR" || name == "UI_ERROR") {
        std::string s = readVmString(vm, args[1]);
        if (s.empty()) s = std::string(name) + " trap invoked";
        if (!host->pendingError.empty()) host->pendingError += " ";
        host->pendingError += s;
        std::string err = std::move(host->pendingError);
        host->pendingError.clear();
        host->lastError = err;
        flushLine(host, std::string_view(err), true);
        host->output.push_back(std::move(err));
        // Quake III aborts on G_ERROR; mirror that by longjmp so the host can
        // report it as a runtime failure instead of continuing.
        std::longjmp(host->errorJump, 1);
    }

    // ----- time ----------------------------------------------------------------
    if (name == "G_MILLISECONDS" || name == "CG_MILLISECONDS" || name == "UI_MILLISECONDS") {
        return static_cast<intptr_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - host->runStart)
            .count());
    }

    // ----- argument access -----------------------------------------------------
    if (name == "G_ARGC" || name == "CG_ARGC" || name == "UI_ARGC") {
        return static_cast<intptr_t>(host->options.args.empty() ? 0 : host->options.args.size());
    }
    if (name == "G_ARGV" || name == "CG_ARGV" || name == "UI_ARGV") {
        const int idx = static_cast<int>(args[1]);
        const char* text = "";
        if (idx >= 0 && static_cast<size_t>(idx) < host->options.args.size()) {
            static thread_local std::string argBuf;
            argBuf = std::to_string(host->options.args[idx]);
            text = argBuf.c_str();
        }
        copyVmBuffer(vm, args[2], static_cast<size_t>(args[3]), text);
        return args[2];
    }

    // ----- cvar stubs (return sensible defaults) ------------------------------
    if (name == "G_CVAR_VARIABLE_STRING_BUFFER" ||
        name == "CG_CVAR_VARIABLESTRINGBUFFER" || name == "UI_CVAR_VARIABLESTRINGBUFFER" ||
        name == "UI_CVAR_INFOSTRINGBUFFER") {
        copyVmBuffer(vm, args[2], static_cast<size_t>(args[3]), "qvm-editor-runtime");
        return 0;
    }
    if (name == "G_CVAR_VARIABLE_INTEGER_VALUE" || name == "UI_CVAR_VARIABLEVALUE") {
        return 0;
    }
    if (name == "G_CVAR_SET" || name == "CG_CVAR_SET" || name == "UI_CVAR_SET") {
        return 0;
    }
    if (name == "G_CVAR_UPDATE" || name == "CG_CVAR_UPDATE" || name == "UI_CVAR_UPDATE") {
        return 0;
    }

    // ----- game-server configstrings ----------------------------------------
    if (name == "G_SET_CONFIGSTRING") {
        host->configStrings[static_cast<int>(args[1])] = readVmString(vm, args[2]);
        return 0;
    }
    if (name == "G_GET_CONFIGSTRING") {
        const auto it = host->configStrings.find(static_cast<int>(args[1]));
        copyVmBuffer(vm, args[2], static_cast<size_t>(args[3]),
                     it == host->configStrings.end() ? "" : it->second.c_str());
        return 0;
    }
    if (name == "G_GET_ENTITY_TOKEN") {
        if (host->nextEntityToken >= host->entityTokens.size()) return 0;
        const std::string& token = host->entityTokens[host->nextEntityToken++];
        copyVmBuffer(vm, args[1], static_cast<size_t>(args[2]), token.c_str());
        return 1;
    }

    if (name == "CG_GETGAMESTATE") {
        // gameState_t is 1024 x int offsets, 16000 bytes of text, then the
        // text byte count.  Seed a tiny, internally consistent server state
        // so cgame can initialize without a running Quake engine.
        constexpr size_t kOffsetsBytes = 1024 * sizeof(int32_t);
        constexpr size_t kStringBytes = 16000;
        constexpr int kGameVersionConfigString = 20;
        const size_t base = static_cast<uint32_t>(args[1]) & static_cast<uint32_t>(vm->dataMask);
        if (base + kOffsetsBytes + kStringBytes + sizeof(int32_t) <= static_cast<size_t>(vm->dataAlloc)) {
            std::memset(vm->dataBase + base, 0, kOffsetsBytes + kStringBytes + sizeof(int32_t));
            const std::string serverInfo = "\\sv_serverid\\1\\mapname\\q3dm1\\g_gametype\\0\\sv_maxclients\\8\\sv_hostname\\QVM Runtime";
            constexpr const char* gameVersion = "baseq3-1";
            const size_t serverInfoOffset = 1;
            const size_t gameVersionOffset = serverInfoOffset + serverInfo.size() + 1;
            std::memcpy(vm->dataBase + base + kOffsetsBytes + serverInfoOffset,
                        serverInfo.c_str(), serverInfo.size() + 1);
            std::memcpy(vm->dataBase + base + kOffsetsBytes + gameVersionOffset,
                        gameVersion, std::strlen(gameVersion) + 1);
            writeVmInt(vm, args[1], 0); // all unpopulated configstrings are empty
            writeVmInt(vm, args[1] + 0 * sizeof(int32_t), static_cast<int32_t>(serverInfoOffset));
            writeVmInt(vm, args[1] + kGameVersionConfigString * sizeof(int32_t),
                       static_cast<int32_t>(gameVersionOffset));
            writeVmInt(vm, args[1] + kOffsetsBytes + kStringBytes,
                       static_cast<int32_t>(gameVersionOffset + std::strlen(gameVersion) + 1));
        }
        return 0;
    }

    // ----- QVM libc imports --------------------------------------------------
    // The stock qagame/cgame/ui modules import these through their syscall
    // assembly table.  They operate on VM offsets, not native pointers.
    if (name == "memset") {
        const size_t dest = static_cast<uint32_t>(args[1]) & static_cast<uint32_t>(vm->dataMask);
        const size_t count = static_cast<size_t>(std::max<intptr_t>(0, args[3]));
        if (dest < static_cast<size_t>(vm->dataAlloc)) {
            std::memset(vm->dataBase + dest, static_cast<unsigned char>(args[2]),
                        std::min(count, static_cast<size_t>(vm->dataAlloc) - dest));
        }
        return args[1];
    }
    if (name == "memcpy") {
        const size_t dest = static_cast<uint32_t>(args[1]) & static_cast<uint32_t>(vm->dataMask);
        const size_t src = static_cast<uint32_t>(args[2]) & static_cast<uint32_t>(vm->dataMask);
        const size_t count = static_cast<size_t>(std::max<intptr_t>(0, args[3]));
        if (dest < static_cast<size_t>(vm->dataAlloc) && src < static_cast<size_t>(vm->dataAlloc)) {
            const size_t available = std::min(static_cast<size_t>(vm->dataAlloc) - dest,
                                              static_cast<size_t>(vm->dataAlloc) - src);
            std::memmove(vm->dataBase + dest, vm->dataBase + src, std::min(count, available));
        }
        return args[1];
    }
    if (name == "strncpy") {
        const size_t dest = static_cast<uint32_t>(args[1]) & static_cast<uint32_t>(vm->dataMask);
        const size_t src = static_cast<uint32_t>(args[2]) & static_cast<uint32_t>(vm->dataMask);
        const size_t count = static_cast<size_t>(std::max<intptr_t>(0, args[3]));
        if (dest < static_cast<size_t>(vm->dataAlloc) && src < static_cast<size_t>(vm->dataAlloc)) {
            const size_t available = std::min(static_cast<size_t>(vm->dataAlloc) - dest,
                                              static_cast<size_t>(vm->dataAlloc) - src);
            std::strncpy(reinterpret_cast<char*>(vm->dataBase + dest),
                         reinterpret_cast<const char*>(vm->dataBase + src),
                         std::min(count, available));
        }
        return args[1];
    }

    // ----- filesystem stubs ---------------------------------------------------
    if (name == "G_FS_FOPEN_FILE" || name == "CG_FS_FOPENFILE" || name == "UI_FS_FOPENFILE") {
        if (args[2]) std::memset(reinterpret_cast<void*>(VM_ArgPtr(args[2], vm)), 0, sizeof(int32_t));
        return 0; // handle = -1
    }
    if (name == "G_FS_READ" || name == "CG_FS_READ" || name == "UI_FS_READ") {
        copyVmBuffer(vm, args[1], static_cast<size_t>(args[2]), "");
        return -1;
    }
    if (name == "G_FS_WRITE" || name == "CG_FS_WRITE" || name == "UI_FS_WRITE") {
        return 0;
    }
    if (name == "G_FS_FCLOSE_FILE" || name == "CG_FS_FCLOSEFILE" || name == "UI_FS_FCLOSEFILE") {
        return 0;
    }
    if (name == "G_SEND_CONSOLE_COMMAND" || name == "CG_SENDCONSOLECOMMAND") {
        copyVmBuffer(vm, args[1], 64, "");
        return 0;
    }

    // Fallback: benign return for every operation the VM shim does not implement.
    return 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Result<RuntimeResult> QvmRuntime::run(const std::filesystem::path& qvmPath,
                                       const RuntimeOptions& options,
                                       OutputCallback onOutput) {
    g_host = nullptr;

    // 1. Read & validate the file.
    std::ifstream f(qvmPath, std::ios::binary);
    if (!f) {
        return Error(ErrorCode::FileNotFound,
                     "cannot open QVM file: " + qvmPath.string());
    }
    f.seekg(0, std::ios::end);
    const auto fileSz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    if (fileSz < 32) {
        return Error(ErrorCode::InvalidHeader, "QVM file too small (" +
                     std::to_string(fileSz) + " bytes)");
    }
    std::vector<uint8_t> bytes(fileSz);
    if (!f.read(reinterpret_cast<char*>(bytes.data()), fileSz)) {
        return Error(ErrorCode::FileReadError,
                     "failed to read QVM file: " + qvmPath.string());
    }
    auto headerRes = QvmHeader::parse(std::span<const uint8_t>(bytes));
    if (headerRes.hasError()) {
        return Error(ErrorCode::InvalidMagic,
                     "bad QVM magic / header: " + headerRes.error().message());
    }
    if (auto headerError = headerRes.value().validate(bytes.size()); headerError.isError()) {
        return headerError;
    }
    // 2. Build host state.
    HostState host;
    host.options = options;
    host.outputCb = onOutput;
    host.runStart = std::chrono::steady_clock::now();

    // 3. Install the C++ host callbacks the bytecode will call back into.
    g_host = &host;

    vm_t vm{};
    // Cover VM_Create as well as VM_Call: malformed instructions can be
    // detected while the interpreter is being prepared, before vmMain runs.
    host.jumpReady = true;
    if (setjmp(host.errorJump)) {
        // reached via longjmp on trap_Error / fatal VM error
        RuntimeResult r;
        r.success = false;
        r.error = std::move(host.lastError);
        if (!host.pendingLine.empty()) {
            flushLine(&host, host.pendingLine, false);
            r.output.push_back(std::move(host.pendingLine));
        }
        if (!host.pendingError.empty()) {
            flushLine(&host, host.pendingError, true);
            r.output.push_back(std::move(host.pendingError));
        }
        r.output.insert(r.output.end(), host.output.begin(), host.output.end());
        r.trace.insert(r.trace.end(), host.trace.begin(), host.trace.end());
        r.syscallCount = host.syscallCount;
        r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - host.runStart)
                            .count();
        // longjmp bypasses VM_Call's normal call-level decrement.  Reset it
        // before releasing the interpreter allocations on this recovery path.
        vm.callLevel = 0;
        VM_Free(&vm);
        g_host = nullptr;
        return r;
    }

    const int rc = VM_Create(&vm, qvmPath.string().c_str(), bytes.data(),
                             static_cast<int>(bytes.size()), qvmSystemCalls);
    if (rc != 0) {
        host.jumpReady = false;
        VM_Free(&vm);
        g_host = nullptr;
        return Error(ErrorCode::RuntimeExecutionFailed,
                     "VM_Create failed for " + qvmPath.string() + ": " +
                     (vm.lastError ? ("0x" + std::to_string(vm.lastError)) : "(null)"));
    }

    // Keep the recovery point armed while the interpreter is executing.  A
    // trap_Error or fatal interpreter error may be raised from inside VM_Call.
    // g_host still points at host; the longjmp path already cleared it.
    vm_t* vmPtr = &vm;

    // forward args: command + up to 12 total parameters
    {
        int n = static_cast<int>(options.args.size());
        if (n == 0) {
            VM_Call(vmPtr, static_cast<int>(options.command));
        } else if (n == 1) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0]);
        } else if (n == 2) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1]);
        } else if (n == 3) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2]);
        } else if (n == 4) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3]);
        } else if (n == 5) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4]);
        } else if (n == 6) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5]);
        } else if (n == 7) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6]);
        } else if (n == 8) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6], options.args[7]);
        } else if (n == 9) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6], options.args[7], options.args[8]);
        } else if (n == 10) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6], options.args[7], options.args[8], options.args[9]);
        } else if (n == 11) {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6], options.args[7], options.args[8], options.args[9],
                    options.args[10]);
        } else {
            VM_Call(vmPtr, static_cast<int>(options.command), options.args[0], options.args[1],
                    options.args[2], options.args[3], options.args[4], options.args[5],
                    options.args[6], options.args[7], options.args[8], options.args[9],
                    options.args[10], options.args[11]);
        }
    }

    host.jumpReady = false;
    VM_Free(&vm);
    g_host = nullptr;
    RuntimeResult r;
    r.success = host.errorCode == 0 && host.lastError.empty();
    r.returnValue = 0;
    r.error = host.lastError;
    r.trace.insert(r.trace.end(), host.trace.begin(), host.trace.end());
    r.syscallCount = host.syscallCount;
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - host.runStart)
                        .count();
    if (!host.pendingLine.empty()) {
        flushLine(&host, host.pendingLine, false);
        r.output.push_back(std::move(host.pendingLine));
    }
    if (!host.pendingError.empty()) {
        flushLine(&host, host.pendingError, true);
        r.output.push_back(std::move(host.pendingError));
    }
    r.output.insert(r.output.end(), host.output.begin(), host.output.end());
    return r;
}

} // namespace qvm
