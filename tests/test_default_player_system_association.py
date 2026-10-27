from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def _function_body(source: str, signature: str) -> str:
    marker = source.find(signature)
    if marker < 0:
        raise AssertionError("missing-signature:%s" % signature)
    brace = source.find("{", marker)
    if brace < 0:
        raise AssertionError("missing-body:%s" % signature)
    depth = 0
    for index, ch in enumerate(source[brace:], start=brace):
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[marker:index + 1]
    raise AssertionError("unclosed-body:%s" % signature)


def _unique_set_as_default_player_definition():
    files = (
        "src/WindowsMain.cpp",
        "src/DefaultAudioPlayerAssociation.cpp",
    )
    hits = []
    signature = "void SetAsDefaultPlayer()"
    for relative in files:
        source = _read(relative)
        start = 0
        while True:
            marker = source.find(signature, start)
            if marker < 0:
                break
            after = source[marker + len(signature):].lstrip()
            if after.startswith("{"):
                hits.append((relative, _function_body(source, signature), source))
            start = marker + len(signature)
    if len(hits) != 1:
        raise AssertionError("unique-definition-count:%s" % len(hits))
    return hits[0]


class DefaultPlayerSystemAssociationContract(unittest.TestCase):
    def test_platform_exposes_effective_association_query(self) -> None:
        header = _read("src/platform/system.h")
        win32 = _read("src/platform/system_win32.cpp")
        macos = _read("src/platform/system_macos.mm")
        self.assertIn("QueryEffectiveFileAssociation(", header)
        self.assertIn("QueryEffectiveFileAssociation(", win32)
        self.assertIn("QueryCurrentDefault", win32)
        self.assertIn("QueryEffectiveFileAssociation(", macos)

    def test_platform_exposes_default_apps_settings_launch(self) -> None:
        header = _read("src/platform/system.h")
        win32 = _read("src/platform/system_win32.cpp")
        macos = _read("src/platform/system_macos.mm")
        self.assertIn("LaunchDefaultAppsSettings(", header)
        self.assertIn("LaunchDefaultAppsSettings(", win32)
        self.assertIn("ms-settings:defaultapps", win32)
        self.assertIn("LaunchDefaultAppsSettings(", macos)

    def test_settings_set_default_does_not_write_protected_userchoice(self) -> None:
        relative, body, _source = _unique_set_as_default_player_definition()
        self.assertNotIn("SetAsDefaultPlayerUnder", body)
        self.assertNotIn("Explorer\\FileExts", body)
        self.assertIn("LaunchDefaultAppsSettings()", body)
        self.assertIn("RegisterDefaultProgramsCapabilities()", body)
        self.assertTrue(relative.endswith("DefaultAudioPlayerAssociation.cpp"))

    def test_settings_default_status_uses_system_association_not_userchoice(self) -> None:
        source = _read("src/DefaultAudioPlayerAssociation.cpp")
        self.assertIn("bool IsRegisteredAsDefault()", source)
        body = _function_body(source, "bool IsRegisteredAsDefault()")
        if "QueryEffectiveFileAssociation(" not in body:
            body = _function_body(source, "bool IsSystemDefaultAudioPlayer()")
        self.assertNotIn("Explorer\\FileExts", body)
        self.assertNotIn("IsRegisteredAsDefaultUnder", body)
        self.assertIn("QueryEffectiveFileAssociation(", body)
        self.assertIn("kProgId", body)
        self.assertIn('constexpr const char kProgId[] = "Auro.AudioFile"', source)

    def test_isolated_registry_helper_still_exists_for_cancel_tests(self) -> None:
        header = _read("src/DefaultAudioPlayerAssociation.h")
        impl = _read("src/DefaultAudioPlayerAssociation.cpp")
        self.assertIn("RegisterDefaultProgramsCapabilitiesUnder(", header)
        self.assertIn("RegisterDefaultProgramsCapabilitiesUnder(", impl)
        self.assertIn("RegisteredApplications", impl)
        self.assertIn("FileAssociations", impl)

    def test_launch_default_apps_settings_fails_closed_before_com_or_url(self) -> None:
        win32 = _read("src/platform/system_win32.cpp")
        self.assertIn("bool LaunchDefaultAppsSettings()", win32)
        body = _function_body(win32, "bool LaunchDefaultAppsSettings()")
        isolation = body.find("PreviewIsolationActive")
        self.assertGreaterEqual(isolation, 0)
        false_return = body.find("return false", isolation)
        self.assertGreater(false_return, isolation)
        for token in ("CoCreateInstance", "LaunchAdvancedAssociationUI", "OpenUrl("):
            token_at = body.find(token)
            self.assertGreater(token_at, false_return, token)


if __name__ == "__main__":
    unittest.main()
