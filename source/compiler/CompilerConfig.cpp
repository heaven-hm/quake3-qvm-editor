#include "qvm/compiler/CompilerConfig.hpp"
#include <algorithm>

namespace qvm {

std::string_view moduleTypeToString(ModuleType type) noexcept {
    switch (type) {
        case ModuleType::QaGame: return "qagame";
        case ModuleType::CGame:  return "cgame";
        case ModuleType::Ui:     return "ui";
        case ModuleType::Custom: return "custom";
        default:                 return "unknown";
    }
}

ModuleType stringToModuleType(std::string_view str) noexcept {
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "qagame" || lower == "game") {
        return ModuleType::QaGame;
    }
    if (lower == "cgame") {
        return ModuleType::CGame;
    }
    if (lower == "ui" || lower == "q3_ui") {
        return ModuleType::Ui;
    }
    return ModuleType::Custom;
}

} // namespace qvm
