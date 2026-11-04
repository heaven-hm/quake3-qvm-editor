#include "qvm/compiler/CompilerConfig.hpp"

#include <cctype>
#include <string_view>

namespace qvm {

std::string_view moduleTypeToString(ModuleType type) noexcept {
    switch (type) {
    case ModuleType::QaGame: return "qagame";
    case ModuleType::CGame:  return "cgame";
    case ModuleType::Ui:     return "ui";
    case ModuleType::Custom: return "custom";
    }
    return "unknown";
}

ModuleType stringToModuleType(std::string_view str) noexcept {
    // Normalize common aliases.
    std::string norm(str);
    for (auto& c : norm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (norm == "qagame" || norm == "game" || norm == "g" || norm == "gamet")
        return ModuleType::QaGame;
    if (norm == "cgame" || norm == "cg" || norm == "clientgame")
        return ModuleType::CGame;
    if (norm == "ui" || norm == "q3_ui" || norm == "u")
        return ModuleType::Ui;
    if (norm == "custom" || norm == "cust")
        return ModuleType::Custom;
    // Default to qagame.
    return ModuleType::QaGame;
}

} // namespace qvm
