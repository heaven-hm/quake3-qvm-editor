#include "qvm/compiler/ModuleConfig.hpp"
#include <algorithm>

namespace qvm {

ModuleFileList ModuleConfig::getModuleFileList(ModuleType type) {
    ModuleFileList list;

    switch (type) {
        case ModuleType::QaGame:
            list.primarySourceDirectory = "game";
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
            list.primarySourceDirectory = "cgame";
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
            list.primarySourceDirectory = "q3_ui";
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
            list.primarySourceDirectory.clear();
            list.defaultOutputBaseName = "custom";
            list.defaultDefines = {"-DQ3_VM"};
            list.defaultRelativeIncludes = {"."};
            break;
    }

    return list;
}

ModuleMapping ModuleConfig::getModuleMapping(ModuleType type) {
    ModuleMapping mapping;
    mapping.sourceFile = "mod.c";

    switch (type) {
        case ModuleType::CGame:
            mapping.syscallFile = "cg_syscalls.asm";
            mapping.outputFile = "cgame.qvm";
            mapping.defaultDefines = {"-DQ3_VM", "-DCGAME"};
            break;

        case ModuleType::QaGame:
            mapping.syscallFile = "g_syscalls.asm";
            mapping.outputFile = "qagame.qvm";
            mapping.defaultDefines = {"-DQ3_VM"};
            break;

        case ModuleType::Ui:
            mapping.syscallFile = "ui_syscalls.asm";
            mapping.outputFile = "ui.qvm";
            mapping.defaultDefines = {"-DQ3_VM"};
            break;

        case ModuleType::Custom:
            mapping.syscallFile = "g_syscalls.asm";
            mapping.outputFile = "custom.qvm";
            mapping.defaultDefines = {"-DQ3_VM"};
            break;
    }

    return mapping;
}

ModuleType ModuleConfig::deduceModuleFromOutputName(std::string_view outputName) {
    std::string lower;
    lower.reserve(outputName.size());
    for (char c : outputName) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    // Match against the filename stem only, and require a token boundary.
    // This prevents names such as "build.qvm", "guide.qvm" or "circuit.qvm"
    // from being misread as the "ui" module.
    const std::string stem = std::filesystem::path(lower).stem().string();

    auto matchesToken = [&](std::string_view token) {
        if (stem == token) {
            return true;
        }
        return stem.size() > token.size() &&
               stem.compare(0, token.size(), token) == 0 &&
               (stem[token.size()] == '_' || stem[token.size()] == '-');
    };

    if (matchesToken("cgame")) {
        return ModuleType::CGame;
    }
    if (matchesToken("qagame") || matchesToken("game")) {
        return ModuleType::QaGame;
    }
    if (matchesToken("ui") || matchesToken("q3_ui")) {
        return ModuleType::Ui;
    }

    return ModuleType::Custom;
}

ModuleType ModuleConfig::deduceModuleFromSourceOrSyscall(std::string_view filename) {
    std::string lower;
    lower.reserve(filename.size());
    for (char c : filename) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    const std::string stem = std::filesystem::path(lower).stem().string();

    auto has = [&](std::string_view token) {
        return lower.find(token) != std::string::npos;
    };
    auto startsWith = [&](std::string_view prefix) {
        return stem.rfind(prefix, 0) == 0;
    };

    if (has("cg_syscalls") || stem == "cgame" || stem == "cg_main" || startsWith("cg_")) {
        return ModuleType::CGame;
    }
    if (has("ui_syscalls") || stem == "ui" || stem == "ui_main" || stem == "q3_ui" || startsWith("ui_")) {
        return ModuleType::Ui;
    }
    if (has("g_syscalls") || stem == "qagame" || stem == "game" || stem == "g_main" || startsWith("g_")) {
        return ModuleType::QaGame;
    }

    // Fallback to loose substring matching (covers descriptive filenames).
    if (has("cgame")) {
        return ModuleType::CGame;
    }
    if (has("qagame")) {
        return ModuleType::QaGame;
    }

    return ModuleType::Custom;
}

std::vector<std::filesystem::path> ModuleConfig::resolveSourceFiles(
    const std::filesystem::path& baseSourceDir,
    const ModuleFileList& fileList) {

    std::vector<std::filesystem::path> resolved;

    std::error_code rootEc;
    const bool hasModuleTree =
        !fileList.primarySourceDirectory.empty() &&
        std::filesystem::is_directory(baseSourceDir / fileList.primarySourceDirectory, rootEc);

    auto searchFile = [&](const std::string& filename) -> std::filesystem::path {
        // When the chosen directory is the common code root, prefer the
        // requested module tree.  Some modern/ioquake-derived SDKs include
        // top-level demos named g_main.c, which are not qagame sources.
        if (hasModuleTree) {
            auto modulePath = baseSourceDir / fileList.primarySourceDirectory / filename;
            if (std::filesystem::exists(modulePath)) {
                return modulePath;
            }
        }

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

        // 3. Subdirectories (game, cgame, ui, q3_ui, qcommon)
        for (const auto& sub : {"game", "cgame", "q3_ui", "ui", "qcommon",
                                "code/game", "code/cgame", "code/q3_ui", "code/ui", "code/qcommon"}) {
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
