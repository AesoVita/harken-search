from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8", errors="replace")


def load_release_checker():
    checker_path = ROOT / "tools" / "check_release_compliance.py"
    spec = importlib.util.spec_from_file_location("aris_release_compliance", checker_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load release checker: {checker_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Stage1ProvenanceGate(unittest.TestCase):
    def test_private_nuendo_research_corpus_is_not_tracked(self) -> None:
        forbidden_patterns = (
            "src/NuendoSki*",
            "src/Nuendo" + "Sound" + "minerWdlClient*",
            "tools/*nuendo_ski*",
            "tools/*nuendo_" + "sound" + "miner*",
            "tests/test_nuendo*ski*",
            "tests/test_steinberg_sdk_adapter*",
        )
        found = []
        for pattern in forbidden_patterns:
            found.extend(
                path.relative_to(ROOT).as_posix()
                for path in ROOT.glob(pattern)
                if path.is_file()
            )
        self.assertEqual([], sorted(found), f"private Nuendo research files remain: {found}")

        cmake = read("CMakeLists.txt")
        main = read("src/main.cpp")
        forbidden_build_tokens = (
            "ARIS_ENABLE_PRIVATE_NUENDO_RESEARCH",
            "ARIS_PRIVATE_NUENDO_SOURCES",
            "AuroNuendoSkiBridge",
            "TrySendNuendoSkiDirectSpotJob",
            "TriggerNuendoImportAudioAction",
            "Nuendo" + "Experimental",
            "/api/projects/" + "active",
            "Nuendo SKI" + " Bridge",
            "install_nuendo_" + "ski_bridge",
        )
        for token in forbidden_build_tokens:
            self.assertNotIn(token, cmake + main)

    def test_public_daw_bridge_registry_is_reaper_only(self) -> None:
        installer = read("src/DawBridgeInstaller.cpp")
        registry_body = installer[
            installer.index("std::vector<Installer> BuildInstallerRegistry()") :
        ]
        self.assertIn("registry.push_back(MakeReaperExtensionInstaller());", registry_body)
        self.assertEqual(1, registry_body.count("registry.push_back("))
        self.assertNotIn("MakeNuendo", installer)
        self.assertNotIn("nuendo-ski", installer.casefold())

    def test_public_surface_has_no_private_binary_analysis_markers(self) -> None:
        markers = (
            "sound" + "minerski.dll",
            "reaper_" + "reawwise.dll",
            "Ghi" + "dra",
            "reverse-" + "engineered",
            "decom" + "pilation",
            "vtable " + "slot",
        )
        scan_roots = (
            ROOT / "src",
            ROOT / "docs" / "behavior",
            ROOT / "docs" / "product",
            ROOT / "BUILD_WINDOWS.md",
            ROOT / "CMakeLists.txt",
        )
        hits: list[str] = []
        for scan_root in scan_roots:
            candidates = [scan_root] if scan_root.is_file() else scan_root.rglob("*")
            for path in candidates:
                if not path.is_file():
                    continue
                text = path.read_text(encoding="utf-8", errors="replace").casefold()
                for marker in markers:
                    if marker.casefold() in text:
                        hits.append(f"{path.relative_to(ROOT).as_posix()}: {marker}")
        self.assertEqual([], hits, "private binary-analysis markers remain:\n" + "\n".join(hits))

    def test_wwise_calls_are_grounded_in_published_waapi_contracts(self) -> None:
        provenance_path = ROOT / "docs" / "process" / "WWISE_WAAPI_PROVENANCE.md"
        self.assertTrue(provenance_path.exists(), "missing public WAAPI provenance note")
        provenance = provenance_path.read_text(encoding="utf-8", errors="replace")
        for token in (
            "ak.wwise.core.audio.import",
            "ak.wwise.core.object.pasteProperties",
            "ak.wwise.core.undo.beginGroup",
            "ak.wwise.core.undo.endGroup",
            "https://www.audiokinetic.com/",
        ):
            self.assertIn(token, provenance)

        source = read("src/WwiseTransferExecute.cpp")
        for token in (
            'args["importOperation"]',
            'args["imports"]',
            'args["source"]',
            'args["targets"]',
        ):
            self.assertIn(token, source)
        for marker in ("Rea" + "Wwise", "reaper_" + "reawwise.dll", "decom" + "pilation"):
            self.assertNotIn(marker.casefold(), source.casefold())

    def test_retired_web_runtime_is_absent_and_remaining_licenses_are_bundled(self) -> None:
        lufs_license = ROOT / "LICENSES" / "Klangfreund-LUFSMeter-MIT.txt"
        reaper_license = ROOT / "LICENSES" / "REAPER-SDK-zlib.txt"
        tweetnacl_notice = ROOT / "LICENSES" / "TweetNaCl-PUBLIC-DOMAIN.txt"
        for path in (lufs_license, reaper_license, tweetnacl_notice):
            self.assertTrue(path.exists(), f"missing provenance artifact: {path.relative_to(ROOT)}")

        self.assertIn("Klangfreund Samuel Gaehwiler", lufs_license.read_text(encoding="utf-8"))
        self.assertIn("Cockos Incorporated", reaper_license.read_text(encoding="utf-8"))
        self.assertIn("Daniel J. Bernstein", tweetnacl_notice.read_text(encoding="utf-8"))

        retired_paths = (
            ROOT / "src" / "WebUIBridge.cpp",
            ROOT / "src" / "WebUIBridge.h",
            ROOT / "LICENSES" / "React-MIT.txt",
        )
        self.assertEqual([], [path.relative_to(ROOT).as_posix() for path in retired_paths if path.exists()])
        self.assertTrue((ROOT / "src" / "webui").is_dir(), "active WebUI twin must remain available")

        notices = read("THIRD_PARTY_NOTICES.md")
        license_index = read("LICENSES/README.md")
        for token in (
            "Klangfreund LUFSMeter",
            "Klangfreund-LUFSMeter-MIT.txt",
            "REAPER-SDK-zlib.txt",
            "TweetNaCl-PUBLIC-DOMAIN.txt",
        ):
            self.assertIn(token, notices + "\n" + license_index)
        for retired_token in (
            "WebUiEmbedded.generated.h",
            "React 19.2.4",
            "ReactDOM 19.2.4",
            "scheduler 0.27.0",
            "React-MIT.txt",
        ):
            self.assertNotIn(retired_token, notices + "\n" + license_index)

    def test_unreviewed_web_experiment_is_removed_without_dropping_native_fader(self) -> None:
        forbidden = (
            "tuizi/package.json",
            "tuizi/vite.config.ts",
            "tuizi/src/App.tsx",
            "tuizi/src/index.css",
        )
        for relative_path in forbidden:
            self.assertFalse((ROOT / relative_path).exists(), f"unreviewed web experiment remains: {relative_path}")

        self.assertTrue((ROOT / "src" / "ImGuiAcrylicFader.cpp").exists())
        self.assertTrue((ROOT / "src" / "ImGuiAcrylicFader.h").exists())
        cmake = read("CMakeLists.txt")
        main = read("src/main.cpp")
        self.assertIn("src/ImGuiAcrylicFader.cpp", cmake)
        self.assertIn('#include "ImGuiAcrylicFader.h"', main)
        self.assertNotIn("tuizi/", cmake + main)

    def test_tracked_build_binaries_have_explicit_provenance(self) -> None:
        provenance = read("thirdparty/BINARY_PROVENANCE.md")
        for artifact in (
            "rlottie.lib",
            "rlottie-image-loader.dll.lib",
            "WebView2Loader.dll",
            "whisper.lib",
            "ggml.lib",
            "ggml-base.lib",
            "ggml-cpu.lib",
        ):
            self.assertIn(f"`{artifact}`", provenance)

    def test_release_checker_has_no_private_quarantine_and_passes_real_tree(self) -> None:
        checker = load_release_checker()
        private_skip_text = "\n".join(str(item) for item in getattr(checker, "SKIP_GLOBS", ()))
        self.assertNotIn("NuendoSki", private_skip_text)
        self.assertNotIn("Sound" + "minerWdlClient", private_skip_text)

        violations = checker.find_violations(repo_root=ROOT)
        runtime_violations = checker.find_runtime_network_violations(repo_root=ROOT)
        license_issues = checker.find_license_directory_issues(ROOT / "LICENSES")
        self.assertEqual([], violations)
        self.assertEqual([], runtime_violations)
        self.assertEqual([], license_issues)


if __name__ == "__main__":
    unittest.main(verbosity=2)
