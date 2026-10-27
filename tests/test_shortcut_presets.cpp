#include <cstdio>

#include "../src/AppState.h"

namespace ImGui {
ImGuiIO& GetIO()
{
    alignas(ImGuiIO) static unsigned char storage[sizeof(ImGuiIO)] = {};
    return *reinterpret_cast<ImGuiIO*>(storage);
}

bool IsKeyPressed(ImGuiKey, bool)
{
    return false;
}

const char* GetKeyName(ImGuiKey)
{
    return "";
}

void MemFree(void*)
{
}
} // namespace ImGui

void WaveformWorker::cancelAllPending()
{
}

static const ShortcutEntry* FindShortcut(ShortcutAction action)
{
    for (int i = 0; i < SHORTCUT_COUNT; ++i) {
        if (g_shortcuts[i].action == action)
            return &g_shortcuts[i];
    }
    return nullptr;
}

static KeyBinding EffectiveBinding(const ShortcutEntry* entry)
{
    if (!entry)
        return {};
    return entry->customKey.key != ImGuiKey_None ? entry->customKey : entry->defaultKey;
}

static bool IsBareArrowBinding(const ShortcutEntry* entry, ImGuiKey key)
{
    const KeyBinding binding = EffectiveBinding(entry);
    return binding.key == key && !binding.ctrl && !binding.shift && !binding.alt;
}

static bool IsAltArrowBinding(const ShortcutEntry* entry, ImGuiKey key)
{
    const KeyBinding binding = EffectiveBinding(entry);
    return binding.key == key && !binding.ctrl && !binding.shift && binding.alt;
}

int main()
{
    int failed = 0;

    ApplyShortcutPreset(ShortcutPreset::ClassicLibrary);
    {
        const ShortcutEntry* next = FindShortcut(ShortcutAction::NextFile);
        const ShortcutEntry* prev = FindShortcut(ShortcutAction::PrevFile);
        if (!next || !prev) {
            std::printf("FAIL: missing shortcut entries\n");
            return 1;
        }
        if (IsBareArrowBinding(next, ImGuiKey_DownArrow) || IsBareArrowBinding(prev, ImGuiKey_UpArrow)) {
            std::printf("FAIL: Classic Library preset steals bare up/down arrows\n");
            return 1;
        }
        if (!IsAltArrowBinding(next, ImGuiKey_DownArrow) || !IsAltArrowBinding(prev, ImGuiKey_UpArrow))
            ++failed;
    }

    ApplyShortcutPreset(ShortcutPreset::Minimal);
    {
        const ShortcutEntry* next = FindShortcut(ShortcutAction::NextFile);
        const ShortcutEntry* prev = FindShortcut(ShortcutAction::PrevFile);
        if (IsBareArrowBinding(next, ImGuiKey_DownArrow) || IsBareArrowBinding(prev, ImGuiKey_UpArrow)) {
            std::printf("FAIL: Minimal preset steals bare up/down arrows\n");
            return 1;
        }
        if (!IsAltArrowBinding(next, ImGuiKey_DownArrow) || !IsAltArrowBinding(prev, ImGuiKey_UpArrow))
            ++failed;
    }

    ApplyShortcutPreset(ShortcutPreset::ProTools);

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
