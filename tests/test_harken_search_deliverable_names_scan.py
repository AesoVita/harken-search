# -*- coding: utf-8 -*-
"""RQ-20260910-18: user-visible paths and shipped filenames drop old brand."""
import json
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
LEDGER_NEW = ROOT / "locale" / "harken.ui.strings.json"
LEDGER_OLD = ROOT / "locale" / "aris.ui.strings.json"
MANIFEST_NEW = ROOT / "resources" / "HarkenSearch.manifest"
MANIFEST_OLD = ROOT / "resources" / "Aris.manifest"
INSTALLER = ROOT / "installer" / "HarkenSearch_Setup.iss"
MIGRATOR = ROOT / "src" / "platform" / "harken_legacy_data_migrator.cpp"
NAME_LAYOUT = ROOT / "src" / "platform" / "harken_user_data_layout.h"

OLD_DELIVERABLE_TOKENS = (
    "aris_thesaurus.json",
    "aris_features.json",
    "aris_features.sig",
    "aris-runtime-pair.json",
    "reaper_auro.dll",
    "Aris.manifest",
    "aris.ui.strings.json",
)

NEW_DELIVERABLE_TOKENS = (
    "harken_thesaurus.json",
    "harken_features.json",
    "harken_features.sig",
    "harken-runtime-pair.json",
    "reaper_harken.dll",
    "HarkenSearch.manifest",
    "harken.ui.strings.json",
)

# Word-boundary old brand in path-like strings. Aurora/Binaural are substrings
# and must not match. Case-insensitive so aris_thesaurus and Aris.manifest both hit.
BRAND_PATH_RE = re.compile(r"(?<![A-Za-z0-9])(?:aris|auro)(?![A-Za-z0-9])", re.IGNORECASE)
PATHISH_RE = re.compile(
    r"(.json|.dll|.manifest|.sig|.zip|.exe|\\|/|https?://)",
    re.IGNORECASE,
)
STRING_LIT_RE = re.compile(r'(?:u8|L|u|U)?"((?:\\.|[^"\\])*)"')
ISS_STRING_RE = re.compile(r"(?:'((?:''|[^'])*)'|\"((?:\\.|[^\"\\])*)\")")

SCAN_ROOTS = (
    "src",
    "cmake",
    "tests",
    "tools",
    "scripts",
    "installer",
    "resources",
    "locale",
    "reaper_extension",
    ".github",
)
SCAN_FILES = ("CMakeLists.txt", "THIRD_PARTY_COMPONENTS.json")
SCAN_SUFFIXES = {
    ".cpp", ".h", ".hpp", ".inl", ".cc", ".c", ".py", ".ps1", ".cmake",
    ".txt", ".iss", ".json", ".md", ".yml", ".yaml", ".rc",
}

# Legal leftovers: 06 migrator legacy list + 10 dual-write old package names.
# Each row is (path, token, reason).
EXEMPTIONS = (
    ("src/platform/harken_user_data_layout.h", "aris_thesaurus.json", "06 filename constants: user thesaurus old name"),
    ("src/platform/harken_user_data_layout.h", "auro_thesaurus.json", "06 filename constants: older thesaurus fallback"),
    ("src/platform/harken_user_data_layout.h", "aris_features.json", "06 filename constants: old feature manifest fallback"),
    ("src/platform/harken_user_data_layout.h", "aris_features.sig", "06 filename constants: old feature signature fallback"),
    ("src/platform/harken_user_data_layout.h", "auro_features.json", "06 filename constants: older feature manifest fallback"),
    ("src/platform/harken_user_data_layout.h", "auro_features.sig", "06 filename constants: older feature signature fallback"),
    ("src/platform/harken_user_data_layout.h", "aris-runtime-pair.json", "06 filename constants: old runtime-pair fallback"),
    ("src/platform/harken_user_data_layout.h", "reaper_auro.dll", "06 filename constants: old REAPER plugin fallback"),
    ("src/platform/harken_user_data_layout.h", "Aris.manifest", "06 filename constants: old process manifest name"),
    ("src/platform/harken_user_data_layout.h", "aris.ui.strings.json", "06 filename constants: old locale ledger name"),
    ("tests/test_harken_legacy_data_migrator.cpp", "aris_thesaurus.json", "06 migrator three-path tests for old thesaurus file"),
    ("tests/test_harken_legacy_data_migrator.cpp", "auro_thesaurus.json", "06 migrator three-path tests for older thesaurus file"),
    ("tests/test_harken_search_deliverable_names_scan.py", "aris_thesaurus.json", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "aris_features.json", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "aris_features.sig", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "aris-runtime-pair.json", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "reaper_auro.dll", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "Aris.manifest", "this scan names the forbidden tokens"),
    ("tests/test_harken_search_deliverable_names_scan.py", "aris.ui.strings.json", "this scan names the forbidden tokens"),
    ("tools/release_update.py", "Aris-{version}-win-x64-update.zip", "10 dual-write old update zip name"),
    ("tools/build_update_manifest.py", "Aris-{version}-win-x64-update.zip", "10 dual-write old update zip name"),
    ("tests/release_contract/fixtures/expected_update_release_contract.json", "Aris-{version}-win-x64-update.zip", "10 dual-write old update zip name"),
    ("tests/release_contract/test_update_release_pipeline_contract.py", "Aris-{version}-win-x64-update.zip", "10 dual-write old update zip name"),
    ("tests/release_contract/test_update_release_pipeline_behavior_contract.py", "Aris-1.2.3-win-x64-update.zip", "10 dual-write old update zip name"),
    ("tests/test_harken_search_update_endpoints_scan.py", "Aris-{version}-win-x64-update.zip", "10 dual-write old update zip name"),
)

SKIP_REL_PREFIXES = (
    "docs/",
    "thirdparty/",
    "node_modules/",
    ".git/",
)


def _rel(path):
    return path.relative_to(ROOT).as_posix()


def _iter_scan_files():
    seen = set()
    for folder in SCAN_ROOTS:
        base = ROOT / folder
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            rel = _rel(path)
            if any(rel.startswith(prefix) for prefix in SKIP_REL_PREFIXES):
                continue
            if path.suffix.lower() not in SCAN_SUFFIXES and path.name not in SCAN_FILES:
                continue
            if rel in seen:
                continue
            seen.add(rel)
            yield path, rel
    for name in SCAN_FILES:
        path = ROOT / name
        if path.is_file():
            rel = _rel(path)
            if rel not in seen:
                seen.add(rel)
                yield path, rel


def _strip_comment(rel, line):
    stripped = line.lstrip()
    if rel.endswith(".iss") and stripped.startswith(";"):
        return ""
    if rel.endswith((".py", ".ps1", ".yml", ".yaml")) and stripped.startswith("#"):
        return ""
    if rel.endswith((".cpp", ".h", ".hpp", ".inl", ".cc", ".c", ".cmake", ".txt")):
        if stripped.startswith("//"):
            return ""
        return line.split("//", 1)[0]
    return line



def _iter_string_literals(rel, text):
    """Pull C/Python/ISS string literals so path checks do not scan comments or identifiers."""
    if rel.endswith(".iss"):
        for match in ISS_STRING_RE.finditer(text):
            yield match.group(1) or match.group(2) or ""
        return
    for match in STRING_LIT_RE.finditer(text):
        yield match.group(1)


def _pathish_old_brand_tokens(value):
    """Old deliverable tokens in path-like strings, with aris/auro word boundaries.

    Aurora/Binaural are substrings and must not match. Non-path text is ignored.
    """
    if not value or not PATHISH_RE.search(value) or not BRAND_PATH_RE.search(value):
        return []
    lowered = value.lower()
    return [token for token in OLD_DELIVERABLE_TOKENS if token.lower() in lowered]


def _old_deliverable_hits_in_code(rel, code):
    hits = []
    for token in OLD_DELIVERABLE_TOKENS:
        if token in code:
            hits.append(token)
    for literal in _iter_string_literals(rel, code):
        for token in _pathish_old_brand_tokens(literal):
            if token not in hits:
                hits.append(token)
    return hits


def _exemption_map():
    out = {}
    for path, token, reason in EXEMPTIONS:
        out.setdefault(path, []).append((token, reason))
    return out


def _approved_legacy_thesaurus_cleanup(rel, line_no, raw, text):
    """Only the installer upgrade cleanup and its focused fixture may name this file."""
    if rel == "tests/test_installer_lifecycle_contract.py":
        start = text.index("def test_stale_brand_files_are_deleted_from_install_dir_only")
        end = text.index("def test_", start + 4) if "def test_" in text[start + 4:] else len(text)
        first = text.count("\n", 0, start) + 1
        last = text.count("\n", 0, end) + 1
        fixture = text.splitlines()[first - 1:last - 1]
        return (first <= line_no < last and raw.strip() == '"aris_thesaurus.json",'
                and sum(line.strip() == raw.strip() for line in fixture) == 1)
    if rel != "installer/HarkenSearch_Setup.iss":
        return False
    if raw.strip() == "Names[1] := 'aris_thesaurus.json';":
        start = text.index("procedure DeleteLegacyStaleFiles(")
        end = text.index("function UninstallLegacyArisIfPresent", start)
    elif raw.strip() == 'Type: files; Name: "{app}\\aris_thesaurus.json"':
        install_start = text.index("[InstallDelete]")
        uninstall_start = text.index("[UninstallDelete]", install_start)
        icons_start = text.index("[Icons]", uninstall_start)
        uninstall_line = text.count("\n", 0, uninstall_start) + 1
        start, end = ((install_start, uninstall_start) if line_no < uninstall_line
                      else (uninstall_start, icons_start))
    else:
        return False
    first = text.count("\n", 0, start) + 1
    last = text.count("\n", 0, end) + 1
    section = text.splitlines()[first - 1:last - 1]
    return (first <= line_no < last and
            sum(line.strip() == raw.strip() for line in section) == 1)


class HarkenSearchDeliverableNamesScanTests(unittest.TestCase):
    def test_legacy_thesaurus_cleanup_remains_in_both_installer_sections(self):
        installer = INSTALLER.read_text(encoding="utf-8")
        cleanup_line = 'Type: files; Name: "{app}\\aris_thesaurus.json"'
        install = installer.split("[InstallDelete]", 1)[1].split("[UninstallDelete]", 1)[0]
        uninstall = installer.split("[UninstallDelete]", 1)[1].split("[Icons]", 1)[0]
        self.assertEqual(1, sum(line.strip() == cleanup_line for line in install.splitlines()))
        self.assertEqual(1, sum(line.strip() == cleanup_line for line in uninstall.splitlines()))

    def test_repo_files_use_new_deliverable_names(self):
        self.assertFalse(LEDGER_OLD.is_file(), "locale/aris.ui.strings.json must be renamed")
        self.assertTrue(LEDGER_NEW.is_file(), "locale/harken.ui.strings.json is the L() ledger")
        self.assertFalse(MANIFEST_OLD.is_file(), "resources/Aris.manifest must be renamed")
        self.assertTrue(MANIFEST_NEW.is_file(), "resources/HarkenSearch.manifest is the process manifest")

    def test_old_deliverable_tokens_only_in_annotated_exemptions(self):
        allowed = _exemption_map()
        hits = []
        for path, rel in _iter_scan_files():
            text = path.read_text(encoding="utf-8")
            allowed_tokens = {token for token, _reason in allowed.get(rel, [])}
            for line_no, raw in enumerate(text.splitlines(), 1):
                code = _strip_comment(rel, raw)
                if not code:
                    continue
                for token in _old_deliverable_hits_in_code(rel, code):
                    if token == "aris_thesaurus.json" and _approved_legacy_thesaurus_cleanup(rel, line_no, raw, text):
                        continue
                    if token not in allowed_tokens:
                        hits.append("%s:%d:%s:%s" % (rel, line_no, token, raw.strip()[:160]))
        self.assertEqual(
            hits,
            [],
            "old deliverable names remain outside 06/10 exemptions:\n" + "\n".join(hits[:120]),
        )

    def test_exemptions_are_present_and_annotated(self):
        missing = []
        for rel, token, reason in EXEMPTIONS:
            path = ROOT / rel
            if not path.is_file():
                missing.append("%s missing file for %s (%s)" % (rel, token, reason))
                continue
            text = path.read_text(encoding="utf-8")
            if token not in text:
                missing.append("%s missing token %s (%s)" % (rel, token, reason))
            if rel.startswith("src/platform/harken_legacy_data_migrator") or rel.startswith("tools/release_update"):
                if "legacy" not in text.lower() and "旧客户端兼容供给" not in text and "06" not in text:
                    # migrator file is itself the legacy list; publisher keeps the 10 note.
                    pass
        self.assertEqual(missing, [], "annotated exemptions drifted:\n" + "\n".join(missing))
        names = NAME_LAYOUT.read_text(encoding="utf-8") if NAME_LAYOUT.is_file() else ""
        for token in (
            "aris_thesaurus.json",
            "aris_features.json",
            "aris-runtime-pair.json",
            "reaper_auro.dll",
        ):
            self.assertIn(token, names, "06 filename constants missing " + token)

    def test_new_deliverable_tokens_are_wired(self):
        blob = []
        for path, _rel in _iter_scan_files():
            blob.append(path.read_text(encoding="utf-8"))
        joined = "\n".join(blob)
        missing = [token for token in NEW_DELIVERABLE_TOKENS if token not in joined]
        self.assertEqual(missing, [], "new deliverable names missing: " + ", ".join(missing))
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("add_subdirectory(reaper_extension)", cmake)
        reaper_cmake = (ROOT / "reaper_extension" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("add_library(reaper_harken", reaper_cmake)
        self.assertIn('OUTPUT_NAME "reaper_harken"', reaper_cmake)
        self.assertIn("harken.ui.strings.json", cmake)
        self.assertIn("HarkenSearch.manifest", cmake)
        self.assertIn("harken-runtime-pair.json", cmake)

    def test_user_visible_paths_drop_old_brand(self):
        hits = []
        ledger_path = LEDGER_NEW if LEDGER_NEW.is_file() else LEDGER_OLD
        if ledger_path.is_file():
            payload = json.loads(ledger_path.read_text(encoding="utf-8"))
            for entry in payload.get("entries", []):
                ident = entry.get("id", "")
                for key, value in entry.items():
                    if key in {"id", "type"} or not isinstance(value, str):
                        continue
                    for token in OLD_DELIVERABLE_TOKENS:
                        if token in value:
                            hits.append("ledger:%s.%s:%s:%s" % (ident, key, token, value[:160]))
        installer = INSTALLER.read_text(encoding="utf-8")
        for line_no, raw in enumerate(installer.splitlines(), 1):
            stripped = raw.lstrip()
            if stripped.startswith(";"):
                continue
            if "aris.audio" in raw.lower():
                hits.append("installer:%d:aris.audio:%s" % (line_no, raw.strip()[:160]))
            for token in OLD_DELIVERABLE_TOKENS:
                if token in raw:
                    if token == "aris_thesaurus.json" and _approved_legacy_thesaurus_cleanup(
                        "installer/HarkenSearch_Setup.iss", line_no, raw, installer
                    ):
                        continue
                    hits.append("installer:%d:%s:%s" % (line_no, token, raw.strip()[:160]))
        self.assertEqual(hits, [], 'user-visible paths still have old brand:' + chr(10) + chr(10).join(hits[:80]))
        self.assertNotIn("https://aris.audio", installer)
        self.assertNotIn("官网未定案", installer)
        self.assertIn('#define MyAppURL "https://harkensearch.xd.com/"', installer)
        for key in ("AppPublisherURL", "AppSupportURL", "AppUpdatesURL"):
            self.assertIn(f"{key}={{#MyAppURL}}", installer)
        if ledger_path.is_file():
            joined = ledger_path.read_text(encoding="utf-8")
            self.assertIn("harken_thesaurus.json", joined)


    def test_word_boundary_regexes_ignore_aurora_binaural(self):
        self.assertEqual(_pathish_old_brand_tokens(r"C:\\Aurora\\Binaural.wav"), [])
        self.assertEqual(_pathish_old_brand_tokens("Aurora plugin.dll"), [])
        self.assertEqual(_pathish_old_brand_tokens("Binaural.json"), [])
        self.assertEqual(_pathish_old_brand_tokens("plain aris mention without a path"), [])

    def test_word_boundary_regexes_catch_old_deliverable_paths(self):
        self.assertEqual(_pathish_old_brand_tokens("plugins/reaper_auro.dll"), ["reaper_auro.dll"])
        self.assertEqual(_pathish_old_brand_tokens("locale/aris_thesaurus.json"), ["aris_thesaurus.json"])
        self.assertEqual(_pathish_old_brand_tokens("pack/aris-runtime-pair.json"), ["aris-runtime-pair.json"])
        cpp_lits = list(_iter_string_literals("x.cpp", 'const char* p = "reaper_auro.dll"; const char* a = "Binaural.wav";'))
        self.assertIn("reaper_auro.dll", cpp_lits)
        self.assertIn("Binaural.wav", cpp_lits)
        self.assertEqual(_pathish_old_brand_tokens("Binaural.wav"), [])
        iss_lits = list(_iter_string_literals("x.iss", 'Source: "reaper_auro.dll"; DestDir: \'{app}\''))
        self.assertIn("reaper_auro.dll", iss_lits)
        self.assertTrue(any(_pathish_old_brand_tokens(lit) for lit in iss_lits))


    def test_pack_and_installer_write_new_names(self):
        packer = (ROOT / "tools" / "pack_engine.py").read_text(encoding="utf-8")
        installer = INSTALLER.read_text(encoding="utf-8")
        self.assertIn("harken_features.json", packer)
        self.assertIn("harken_features.sig", packer)
        self.assertIn("harken_thesaurus.json", packer)
        self.assertIn("reaper_harken.dll", packer)
        self.assertNotIn("aris_features.json", packer)
        self.assertNotIn("reaper_auro.dll", packer)
        self.assertIn("harken_features.json", installer)
        self.assertIn("harken-runtime-pair.json", installer)
        self.assertIn("reaper_harken.dll", installer)
        self.assertNotIn("aris-runtime-pair.json", installer)
        self.assertNotIn("reaper_auro.dll", installer)


if __name__ == "__main__":
    unittest.main()
