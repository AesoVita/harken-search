#include <cassert>
#include <map>
#include <string>

#include "../src/SettingsMigration.h"

int main() {
    // v1.0.38 stored three retired tempo bindings at slots 31..33.
    assert(PersistedShortcutIndex(30) == 30);
    assert(PersistedShortcutIndex(31) == 34); // SendToDAW1
    assert(PersistedShortcutIndex(34) == 37); // SendToDAW4
    assert(PersistedShortcutIndex(35) == 38); // SpotToDAW
    assert(PersistedShortcutIndex(37) == 40); // TapTempo

    std::map<std::string, std::string> settings;
    assert(PreferredOutputSuffixSetting(settings) == nullptr);
    settings["keepAuroOutputProcessingSuffix"] = "0";
    assert(*PreferredOutputSuffixSetting(settings) == "0");
    settings["keepHarkenSearchOutputProcessingSuffix"] = "1";
    assert(*PreferredOutputSuffixSetting(settings) == "1");
    settings["keepHarkenSearchOutputProcessingSuffix"] = "";
    assert(*PreferredOutputSuffixSetting(settings) == ""); // present new key wins
}
