#pragma once

#include <string>

// v1.0.38 persisted shortcut positions 31..33 for the removed tempo actions.
// Keep those positions reserved so older DAW bindings cannot attach to a
// different action. The shortcut table and enum still contain no tempo action.
template <typename Action>
constexpr int PersistedShortcutIndex(Action action) {
    const int index = static_cast<int>(action);
    return index < 31 ? index : index + 3;
}

template <typename SettingsMap>
const std::string* PreferredOutputSuffixSetting(const SettingsMap& settings) {
    const auto current = settings.find("keepHarkenSearchOutputProcessingSuffix");
    if (current != settings.end()) return &current->second;
    const auto legacy = settings.find("keepAuroOutputProcessingSuffix");
    return legacy == settings.end() ? nullptr : &legacy->second;
}
