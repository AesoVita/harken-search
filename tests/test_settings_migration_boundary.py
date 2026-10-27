"""Guard the settings load/save connection to historical shortcut slots."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
SHORTCUTS = (ROOT / "src/AppState.cpp").read_text(encoding="utf-8")


class SettingsMigrationBoundaryTests(unittest.TestCase):
    def test_assert_based_release_tests_keep_checks_on_all_compilers(self):
        cmake = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
        for target in ("TestSettingsMigration", "TestFloatingWindowCorners"):
            with self.subTest(target=target):
                block = cmake.split(f"add_executable({target} ", 1)[1].split(
                    f"add_test(NAME {target} ", 1
                )[0]
                self.assertIn(f"target_compile_options({target} PRIVATE /W3 /utf-8 /UNDEBUG)", block)
                self.assertRegex(block, re.compile(
                    rf"else\(\)\s+target_compile_options\({target} PRIVATE -UNDEBUG\)"
                ))

    def test_historical_daw_slot_is_kept_for_load_and_save(self):
        table = SHORTCUTS.split("ShortcutEntry g_shortcuts[] = {", 1)[1].split("};", 1)[0]
        actions = re.findall(r"ShortcutAction::(\w+)", table)
        self.assertEqual(actions.index("SendToDAW1"), 31)
        self.assertNotIn("TempoUp", actions)
        load = MAIN.split("// Load custom shortcuts", 1)[1].split("// Load folder shortcuts", 1)[0]
        save = MAIN.split("// Save custom shortcuts", 1)[1].split("// Save folder shortcuts", 1)[0]
        for block in (load, save):
            self.assertIn("PersistedShortcutIndex(g_shortcuts[i].action)", block)
            self.assertNotIn('std::to_string(i) + "_"', block)

    def test_suffix_fallback_is_wired_to_load_and_new_key_is_saved(self):
        load = MAIN.split("void LoadAppSettings(AppState& state)", 1)[1].split("// Tab presets (F-033)", 1)[0]
        save = MAIN.split("bool TrySaveAppSettings(const AppState& state)", 1)[1].split("void SaveAppSettings", 1)[0]
        self.assertIn("PreferredOutputSuffixSetting(kv)", load)
        self.assertIn('items.push_back({"keepHarkenSearchOutputProcessingSuffix"', save)
        self.assertNotIn('items.push_back({"keepAuroOutputProcessingSuffix"', save)


if __name__ == "__main__":
    unittest.main()
