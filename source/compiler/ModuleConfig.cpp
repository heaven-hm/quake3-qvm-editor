#include "qvm/compiler/ModuleConfig.hpp"
#include <algorithm>

namespace qvm {

ModuleFileList ModuleConfig::getModuleFileList(ModuleType type) {
    ModuleFileList list;

    switch (type) {
        case ModuleType::QaGame:
            list.defaultOutputBaseName = "qagame";
            list.defaultDefines = {"-DQ3_VM"};
            list.defaultRelativeIncludes = {"cgame", "game", "q3_ui", "."};
            list.entryPointFile = "g_main.c";
            list.cFiles = {
                "ai_chat.c", "ai_cmd.c", "ai_dmnet.c", "ai_dmq3.c", "ai_main.c",
                "ai_team.c", "ai_vcmd.c", "bg_misc.c", "bg_pmove.c", "bg_slidemove.c",
                "g_active.c", "g_arenas.c", "g_bot.c", "g_client.c", "g_cmds.c",
                "g_combat.c", "g_items.c", "g_mem.c", "g_misc.c", "g_missile.c",
                "g_mover.c", "g_session.c", "g_spawn.c", "g_svcmds.c", "g_target.c",
                "g_team.c", "g_trigger.c", "g_utils.c", "g_weapon.c", "q_math.c",
                "q_shared.c"
            };
            list.asmFiles = {"g_syscalls.asm"};
            break;

        case ModuleType::CGame:
            list.defaultOutputBaseName = "cgame";
            list.defaultDefines = {"-DQ3_VM", "-DCGAME"};
            list.defaultRelativeIncludes = {"cgame", "game", "q3_ui", "."};
            list.entryPointFile = "cg_main.c";
            list.cFiles = {
                "bg_misc.c", "bg_pmove.c", "bg_slidemove.c", "q_math.c", "q_shared.c",
                "cg_consolecmds.c", "cg_draw.c", "cg_drawtools.c", "cg_effects.c",
                "cg_ents.c", "cg_event.c", "cg_info.c", "cg_localents.c", "cg_marks.c",
                "cg_players.c", "cg_playerstate.c", "cg_predict.c", "cg_scoreboard.c",
                "cg_servercmds.c", "cg_snapshot.c", "cg_view.c", "cg_weapons.c"
            };
            list.asmFiles = {"cg_syscalls.asm"};
            break;

        case ModuleType::Ui:
            list.defaultOutputBaseName = "ui";
            list.defaultDefines = {"-DQ3_VM"};
            list.defaultRelativeIncludes = {"cgame", "game", "q3_ui", "ui", "."};
            list.entryPointFile = "ui_main.c";
            list.cFiles = {
                "bg_misc.c", "q_math.c", "q_shared.c", "ui_addbots.c", "ui_atoms.c",
                "ui_cdkey.c", "ui_cinematics.c", "ui_confirm.c", "ui_connect.c",
                "ui_controls2.c", "ui_credits.c", "ui_demo2.c", "ui_display.c",
                "ui_gameinfo.c", "ui_ingame.c", "ui_menu.c", "ui_mfield.c", "ui_mods.c",
                "ui_network.c", "ui_options.c", "ui_playermodel.c", "ui_players.c",
                "ui_playersettings.c", "ui_preferences.c", "ui_qmenu.c", "ui_removebots.c",
                "ui_serverinfo.c", "ui_servers2.c", "ui_setup.c", "ui_sound.c",
                "ui_sparena.c", "ui_specifyserver.c", "ui_splevel.c", "ui_sppostgame.c",
                "ui_spskill.c", "ui_startserver.c", "ui_team.c", "ui_teamorders.c",
                "ui_video.c"
            };
            list.asmFiles = {"ui_syscalls.asm"};
            break;

        case ModuleType::Custom:
            list.defaultOutputBaseName = "custom";
            list.defaultDefines = {"-DQ3_VM"};
            list.defaultRelativeIncludes = {"."};
            break;
    }

    return list;
}

std::vector<std::filesystem::path> ModuleConfig::resolveSourceFiles(
    const std::filesystem::path& baseSourceDir,
    const ModuleFileList& fileList) {

    std::vector<std::filesystem::path> resolved;

    auto searchFile = [&](const std::string& filename) -> std::filesystem::path {
        // 1. Direct path in base source directory
        auto direct = baseSourceDir / filename;
        if (std::filesystem::exists(direct)) {
            return direct;
        }

        // 2. Filename only (strip leading ../)
        std::string stripped = filename;
        if (size_t pos = stripped.rfind('/'); pos != std::string::npos) {
            stripped = stripped.substr(pos + 1);
        }

        auto simple = baseSourceDir / stripped;
        if (std::filesystem::exists(simple)) {
            return simple;
        }

        // 3. Subdirectories (game, cgame, ui, q3_ui)
        for (const auto& sub : {"game", "cgame", "q3_ui", "ui", "code/game", "code/cgame", "code/q3_ui", "code/ui"}) {
            auto subPath = baseSourceDir / sub / stripped;
            if (std::filesystem::exists(subPath)) {
                return subPath;
            }
        }

        return direct; // return fallback path
    };

    if (!fileList.entryPointFile.empty()) {
        resolved.push_back(searchFile(fileList.entryPointFile));
    }

    for (const auto& cFile : fileList.cFiles) {
        resolved.push_back(searchFile(cFile));
    }

    return resolved;
}

} // namespace qvm
