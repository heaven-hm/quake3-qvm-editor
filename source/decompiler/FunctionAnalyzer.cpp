#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include <iomanip>
#include <sstream>

namespace qvm {

static int32_t cMul(int32_t a, int32_t b) {
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t v = (ua * ub) & 0xffffffff;
    if (v > 0x7fffffff) {
        return -static_cast<int32_t>(0x100000000ULL - v);
    }
    return static_cast<int32_t>(v);
}

int32_t FunctionAnalyzer::computeFunctionHash(
    const std::vector<QvmInstruction>& instructions,
    uint32_t startIndex,
    uint32_t endIndex) {

    if (startIndex >= instructions.size() || startIndex > endIndex) {
        return 0;
    }

    uint8_t firstOp = static_cast<uint8_t>(instructions[startIndex].opcode);
    int32_t value = static_cast<int32_t>(firstOp) << 7;

    for (uint32_t i = startIndex; i <= endIndex && i < instructions.size(); ++i) {
        uint8_t op = static_cast<uint8_t>(instructions[i].opcode);
        value = cMul(1000003, value) ^ static_cast<int32_t>(op);
    }

    uint32_t count = (endIndex >= startIndex) ? (endIndex - startIndex + 1) : 0;
    value = value ^ static_cast<int32_t>(count);
    if (value == -1) {
        value = -2;
    }
    return value;
}

std::string_view FunctionAnalyzer::resolveSyscall(int32_t trapNumber, ModuleType moduleType) noexcept {
    // Common standard library traps shared across VM modules
    switch (trapNumber) {
        case -101: return "memset";
        case -102: return "memcpy";
        case -103: return "strncpy";
        case -104: return "sin";
        case -105: return "cos";
        case -106: return "atan2";
        case -107: return "sqrt";
        case -108: return "floor";
        case -109: return "ceil";
        case -110: return "testprintint";
        case -111: return "testprintfloat";
        case -112: return "acos";
        default: break;
    }

    if (moduleType == ModuleType::QaGame) {
        switch (trapNumber) {
            case -1:  return "trap_Printf";
            case -2:  return "trap_Error";
            case -3:  return "trap_Milliseconds";
            case -4:  return "trap_Cvar_Register";
            case -5:  return "trap_Cvar_Update";
            case -6:  return "trap_Cvar_Set";
            case -7:  return "trap_Cvar_VariableIntegerValue";
            case -8:  return "trap_Cvar_VariableStringBuffer";
            case -9:  return "trap_Argc";
            case -10: return "trap_Argv";
            case -11: return "trap_FS_FOpenFile";
            case -12: return "trap_FS_Read";
            case -13: return "trap_FS_Write";
            case -14: return "trap_FS_FCloseFile";
            case -15: return "trap_SendConsoleCommand";
            case -16: return "trap_LocateGameData";
            case -17: return "trap_DropClient";
            case -18: return "trap_SendServerCommand";
            case -19: return "trap_SetConfigstring";
            case -20: return "trap_GetConfigstring";
            case -21: return "trap_GetUserinfo";
            case -22: return "trap_SetUserinfo";
            case -23: return "trap_GetServerinfo";
            case -24: return "trap_SetBrushModel";
            case -25: return "trap_Trace";
            case -26: return "trap_PointContents";
            case -27: return "trap_InPVS";
            case -28: return "trap_InPVSIgnorePortals";
            case -29: return "trap_AdjustAreaPortalState";
            case -30: return "trap_AreasConnected";
            case -31: return "trap_LinkEntity";
            case -32: return "trap_UnlinkEntity";
            case -33: return "trap_EntitiesInBox";
            case -34: return "trap_EntityContact";
            case -35: return "trap_BotAllocateClient";
            case -36: return "trap_BotFreeClient";
            case -37: return "trap_GetUsercmd";
            case -38: return "trap_GetEntityToken";
            case -39: return "trap_FS_GetFileList";
            case -40: return "trap_DebugPolygonCreate";
            case -41: return "trap_DebugPolygonDelete";
            case -42: return "trap_RealTime";
            case -43: return "trap_SnapVector";
            case -44: return "trap_TraceCapsule";
            case -45: return "trap_EntityContactCapsule";
            case -46: return "trap_FS_Seek";
            default: break;
        }
    } else if (moduleType == ModuleType::CGame) {
        switch (trapNumber) {
            case -1:  return "trap_Print";
            case -2:  return "trap_Error";
            case -3:  return "trap_Milliseconds";
            case -4:  return "trap_Cvar_Register";
            case -5:  return "trap_Cvar_Update";
            case -6:  return "trap_Cvar_Set";
            case -7:  return "trap_Cvar_VariableStringBuffer";
            case -8:  return "trap_Argc";
            case -9:  return "trap_Argv";
            case -10: return "trap_Args";
            case -11: return "trap_FS_FOpenFile";
            case -12: return "trap_FS_Read";
            case -13: return "trap_FS_Write";
            case -14: return "trap_FS_FCloseFile";
            case -15: return "trap_SendConsoleCommand";
            case -16: return "trap_AddCommand";
            case -17: return "trap_SendClientCommand";
            case -18: return "trap_UpdateScreen";
            case -19: return "trap_CM_LoadMap";
            case -20: return "trap_CM_NumInlineModels";
            case -21: return "trap_CM_InlineModel";
            case -22: return "trap_CM_LoadModel";
            case -23: return "trap_CM_TempBoxModel";
            case -24: return "trap_CM_PointContents";
            case -25: return "trap_CM_TransformedPointContents";
            case -26: return "trap_CM_BoxTrace";
            case -27: return "trap_CM_TransformedBoxTrace";
            case -28: return "trap_CM_MarkFragments";
            case -29: return "trap_S_StartSound";
            case -30: return "trap_S_StartLocalSound";
            case -31: return "trap_S_ClearLoopingSounds";
            case -32: return "trap_S_AddLoopingSound";
            case -33: return "trap_S_UpdateEntityPosition";
            case -34: return "trap_S_Respatialize";
            case -35: return "trap_S_RegisterSound";
            case -36: return "trap_S_StartBackgroundTrack";
            case -37: return "trap_R_LoadWorldMap";
            case -38: return "trap_R_RegisterModel";
            case -39: return "trap_R_RegisterSkin";
            case -40: return "trap_R_RegisterShader";
            case -41: return "trap_R_ClearScene";
            case -42: return "trap_R_AddRefEntityToScene";
            case -43: return "trap_R_AddPolyToScene";
            case -44: return "trap_R_AddLightToScene";
            case -45: return "trap_R_RenderScene";
            case -46: return "trap_R_SetColor";
            case -47: return "trap_R_DrawStretchPic";
            case -48: return "trap_R_ModelBounds";
            default: break;
        }
    } else if (moduleType == ModuleType::Ui) {
        switch (trapNumber) {
            case -1:  return "trap_Error";
            case -2:  return "trap_Print";
            case -3:  return "trap_Milliseconds";
            case -4:  return "trap_Cvar_Set";
            case -5:  return "trap_Cvar_VariableValue";
            case -6:  return "trap_Cvar_VariableStringBuffer";
            case -7:  return "trap_Cvar_SetValue";
            case -8:  return "trap_Cvar_Reset";
            case -9:  return "trap_Cvar_Create";
            case -10: return "trap_Cvar_InfoStringBuffer";
            case -11: return "trap_Argc";
            case -12: return "trap_Argv";
            case -13: return "trap_Cmd_ExecuteText";
            case -14: return "trap_FS_FOpenFile";
            case -15: return "trap_FS_Read";
            case -16: return "trap_FS_Write";
            case -17: return "trap_FS_FCloseFile";
            case -18: return "trap_FS_GetFileList";
            case -19: return "trap_R_RegisterModel";
            case -20: return "trap_R_RegisterSkin";
            case -21: return "trap_R_RegisterShaderNoMip";
            case -22: return "trap_R_ClearScene";
            case -23: return "trap_R_AddRefEntityToScene";
            case -24: return "trap_R_AddPolyToScene";
            case -25: return "trap_R_AddLightToScene";
            case -26: return "trap_R_RenderScene";
            case -27: return "trap_R_SetColor";
            case -28: return "trap_R_DrawStretchPic";
            case -29: return "trap_UpdateScreen";
            case -30: return "trap_CM_LerpTag";
            case -31: return "trap_CM_LoadModel";
            case -32: return "trap_S_RegisterSound";
            case -33: return "trap_S_StartLocalSound";
            case -34: return "trap_Key_KeynumToStringBuf";
            case -35: return "trap_Key_GetBindingBuf";
            case -36: return "trap_Key_SetBinding";
            case -37: return "trap_Key_IsDown";
            case -38: return "trap_Key_GetOverstrikeMode";
            case -39: return "trap_Key_SetOverstrikeMode";
            case -40: return "trap_Key_ClearStates";
            case -41: return "trap_Key_GetCatcher";
            case -42: return "trap_Key_SetCatcher";
            case -43: return "trap_GetClipboardData";
            case -44: return "trap_GetGlconfig";
            case -45: return "trap_GetClientState";
            case -46: return "trap_GetConfigString";
            default: break;
        }
    }

    return {};
}

// Known baseq3 function hashes mapping
static const std::unordered_map<int32_t, std::string>& getKnownHashMap() {
    static const std::unordered_map<int32_t, std::string> sKnownHashes = {
        { 0x6e8ca3f5, "vmMain" },
        { 0x071387be, "G_Printf" },
        { -0x4cc921fd, "G_Error" },
        { -0x68df037a, "G_FindTeams" },
        { -0x64bac70e, "G_RemapTeamShaders" },
        { -0x61644230, "G_RegisterCvars" },
        { -0x2007089d, "G_UpdateCvars" },
        { 0x17649847, "G_InitGame" },
        { -0x3ec1aac7, "G_ShutdownGame" },
        { -0x270db2c0, "Com_Error" },
        { -0x2fb55fc5, "Com_Printf" },
        { 0x2a2fe522, "AddTournamentPlayer" },
        { -0x0fad74e2, "RemoveTournamentLoser" },
        { -0x390fbee6, "AdjustTournamentScores" },
        { 0x02fe7c73, "SortRanks" },
        { -0x043006ce, "CalculateRanks" },
        { 0x2a15eaa2, "SendScoreboardMessageToAllClients" },
        { 0x49b7ac1c, "MoveClientToIntermission" },
        { 0x3f4c0dc0, "FindIntermissionPoint" },
        { 0x1202a61b, "BeginIntermission" },
        { -0x30226cea, "ExitLevel" },
        { 0x48ae9695, "G_LogPrintf" },
        { -0x3d530965, "LogExit" },
        { -0x0c2e6bbc, "CheckIntermissionExit" },
        // Common C runtime
        { -0x04bec0e3, "memset" },
        { -0x472d498c, "memcpy" },
        { -0x5d3c407d, "strncpy" },
        { 0x03ccc723, "sin" },
        { 0x2574254d, "cos" },
        { 0x020e861a, "atan2" },
        { 0x01a08c4e, "sqrt" },
        { -0x3619ff05, "floor" },
        { 0x09ea96f5, "ceil" }
    };
    return sKnownHashes;
}

std::vector<FunctionInfo> FunctionAnalyzer::analyzeFunctions(
    const std::vector<QvmInstruction>& instructions,
    ModuleType moduleType) {

    std::vector<FunctionInfo> functions;
    const auto& knownMap = getKnownHashMap();

    // 1. Find all OP_ENTER points
    std::vector<uint32_t> enterIndices;
    for (size_t i = 0; i < instructions.size(); ++i) {
        if (instructions[i].opcode == Opcode::Enter) {
            enterIndices.push_back(static_cast<uint32_t>(i));
        }
    }

    if (enterIndices.empty()) {
        return functions;
    }

    // 2. Build function ranges
    for (size_t i = 0; i < enterIndices.size(); ++i) {
        uint32_t startIdx = enterIndices[i];
        uint32_t endIdx = (i + 1 < enterIndices.size()) ?
            (enterIndices[i + 1] - 1) : static_cast<uint32_t>(instructions.size() - 1);

        FunctionInfo func;
        func.startIndex = startIdx;
        func.endIndex = endIdx;
        func.startOffset = instructions[startIdx].offset;
        func.endOffset = instructions[endIdx].offset;
        func.stackFrameSize = instructions[startIdx].operand;

        func.hash = computeFunctionHash(instructions, startIdx, endIdx);

        // Check hash map
        auto it = knownMap.find(func.hash);
        if (it != knownMap.end()) {
            func.name = it->second;
            func.isKnown = true;
        } else if (startIdx == 0) {
            // First function in Quake 3 QVM is ALWAYS vmMain
            func.name = "vmMain";
            func.isKnown = true;
        } else {
            std::stringstream ss;
            ss << "func_" << std::setw(6) << std::setfill('0') << std::hex << func.startOffset;
            func.name = ss.str();
            func.isKnown = false;
        }

        // Scan for calls and syscalls inside function
        for (uint32_t k = startIdx; k <= endIdx; ++k) {
            if (instructions[k].opcode == Opcode::Call) {
                // Preceding instruction usually loads the target address into the stack via OP_CONST
                if (k > startIdx && instructions[k - 1].opcode == Opcode::Const) {
                    int32_t target = instructions[k - 1].operand;
                    if (target < 0) {
                        auto scName = resolveSyscall(target, moduleType);
                        if (!scName.empty()) {
                            func.invokedSyscalls.push_back(std::string(scName));
                        } else {
                            func.invokedSyscalls.push_back("trap_" + std::to_string(-target));
                        }
                    } else {
                        func.calledAddresses.push_back(target);
                    }
                }
            }
        }

        functions.push_back(func);
    }

    return functions;
}

std::string FunctionAnalyzer::resolveFunctionName(int32_t address, const std::vector<FunctionInfo>& functions) {
    if (address < 0) {
        return "trap_" + std::to_string(-address);
    }
    for (const auto& f : functions) {
        if (static_cast<int32_t>(f.startOffset) == address ||
            static_cast<int32_t>(f.startIndex) == address) {
            return f.name;
        }
    }
    std::stringstream ss;
    ss << "func_0x" << std::hex << address;
    return ss.str();
}

} // namespace qvm
