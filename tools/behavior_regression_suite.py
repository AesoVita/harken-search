#!/usr/bin/env python3
"""List or run Aris behavior-contract regression tests."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = Path(os.environ.get("ARIS_BUILD_DIR", ROOT / "build_dbg")).resolve()
PYTHON = sys.executable
DAW_BRIDGE_INSTALLER_TEST = "|TestDawBridgeInstaller" if os.name == "nt" else ""
PROCESS_LAUNCH_TEST = "|TestPlatformProcessLaunch" if os.name == "nt" else ""


@dataclass(frozen=True)
class BehaviorModule:
    key: str
    contract: str
    ctest_regex: str
    diagnostics: str


MODULES = [
    BehaviorModule("search-query", "docs/behavior/modules/search-query.md", "TestSearchSemanticMatrix|TestSearchQueryTrigram|TestSearchRecall|TestThesaurusBidirectional", "SearchLane coverage assertion"),
    BehaviorModule("search-execution", "docs/behavior/modules/search-execution.md", "TestDbSearchFacets|TestTrigramE2E|TestBrowserSearch|TestFuzzySearchPrd", "Database.searchFiles duration and lane assertions"),
    BehaviorModule("scan-indexing", "docs/behavior/modules/scan-indexing.md", "TestScanRenameQa|TestBackfillRecoveryWorker|TestScanWaveformCacheContract|TestScanPauseResume|TestScanPauseResumeContract", "Scan pause/resume, rollback, and flush integrity assertions"),
    BehaviorModule("similar-sounds", "docs/behavior/modules/similar-sounds.md", "TestSimilaritySearch|TestFeatureExtractor|TestSimilarityWorkflow|TestSimilarityQueryService|TestSimilarSoundsWindowContract|TestLocalization", "visual/listening review remains human-only"),
    BehaviorModule("waveform", "docs/behavior/modules/waveform.md", "TestScanWaveformCacheContract|TestWaveformPresentationPolicy|TestImportPrecomputeWaveform", "MiniWave and scan-cache runtime assertions"),
    BehaviorModule("cover", "docs/behavior/modules/cover.md", "TestCoverPathNorm|TestCoverLaneOrder|TestCoverClickPerf|TestCoverDiagnostics", "CoverLane / Cover.TextureUploaded runtime assertions"),
    BehaviorModule("playback-library-lifecycle", "docs/behavior/modules/playback-library-lifecycle.md", "TestSwitchDatabaseBoundary|TestStartupReconciliation|TestOggPlaybackSupport", "Audio.loadFile and switch-stage diagnostics"),
    BehaviorModule("audioengine", "docs/behavior/modules/audioengine.md", "TestOggPlaybackSupport|TestWmaDecodeSupport|TestCenterSense|TestTimeStretch|TestTimeStretch2|TestChannelRoutingSettings|TestMultichannelMeter|TestProcessedFileExport|TestSingleChannelExport|TestSilenceSkipGate", "Audio.loadFile / export / device-switch diagnostics assertions"),
    BehaviorModule("ui-orchestration", "docs/behavior/modules/ui-orchestration.md", "TestSearchWorkspace|TestSidebarOrder|TestResultActionMenuModel|TestShortcutPresets|TestSwitchDatabaseBoundary|TestUserActionLog", "selection / shortcut / menu / user-action-log orchestration assertions"),
    BehaviorModule("folder-browser", "docs/behavior/modules/folder-browser.md", "TestBrowserSearch|TestOpenWith", "browser no-DB audition, system-tree routing, and browser-path handoff assertions"),
    BehaviorModule("metadata-tags", "docs/behavior/modules/metadata-tags.md", "TestMetadataFields|TestTagSystem|TestTagVocabularyMapper|TestMultiTabSync", "metadata patch success/rejection diagnostics assertions"),
    BehaviorModule("playlist-organization", "docs/behavior/modules/playlist-organization.md", "TestSubPlaylists|TestPlaylistFolderTree|TestSetPlaylistParent|TestPlaylistQuickBarMotion", "playlist parent success/reject diagnostics assertions"),
    BehaviorModule("delivery-export", "docs/behavior/modules/delivery-export.md", "TestExportRegionPreserve|TestProcessedFileExport|TestVoiceDeliveryAcceptance", "export start/completion/failure diagnostics assertions"),
    BehaviorModule("editor-handoff", "docs/behavior/modules/editor-handoff.md", f"TestSendToTargetSurfaceModel|TestCubaseDirectInsertContract|TestResultActionMenuModel|TestSendToTargetUiContract{DAW_BRIDGE_INSTALLER_TEST}", "editor-handoff capability/preflight/history truth plus Steinberg direct-insert adapter diagnostics/assertions"),
    BehaviorModule("settings-startup", "docs/behavior/modules/settings-startup.md", "TestAppSettingsDefaults|TestOutputDeviceComboItem|TestStartupReconciliation|TestProductEdition|TestSettingsEncryption|TestFeatureManifest", "startup branch diagnostics source-contract assertions"),
    BehaviorModule("database-cache-architecture", "docs/behavior/modules/database-cache-architecture.md", "TestDatabaseFeatures|TestStartupReconciliation|TestScanWaveformCacheContract|TestSearchWorkspace|TestUserActionLog|TestExportRegionPreserve", "DB package metrics, sidecar cache state, current workspace/action/export evidence; durable snapshot/activity DB remains follow-up"),
    BehaviorModule("coverwall-lab", "docs/behavior/modules/coverwall-lab.md", "TestCoverWallLabContract", "GUI visual smoke remains human-required"),
    BehaviorModule("cover-replacement-foundation", "docs/behavior/modules/cover-replacement-foundation.md", "TestCoverReplacementFoundation", "replacement identity/storage contract remains stable before wider integration"),
    BehaviorModule("cover-replacement-browse-surface", "docs/behavior/modules/cover-replacement-browse-surface.md", "TestCoverBrowseSurfaceState|TestDatabaseFeatures|TestCoverClickPerf|TestBackgroundIoPolicy", "folder-tree scope, slider stability, and preview parity still require additional targeted/manual proof after baseline regression"),
    BehaviorModule("cover-replacement-retirement", "docs/behavior/modules/cover-replacement-retirement.md", "TestCoverReplacementFoundation|TestCoverReplacementIntegration|TestDatabaseFeatures|TestBackgroundIoPolicy", "integrated retirement probe on harness_multi must prove no-filter / keyword / path / cover prime-repeat, preview parity, rollback, rebuild, and legacy fallback audit after each retirement cut"),
    BehaviorModule(
        "platform-services",
        "docs/behavior/modules/platform-services.md",
        f"TestStage6PlatformBoundary|TestStage6PerformanceBaseline|TestBehaviorContractCheck|TestMetadataReaderPathEncoding|TestMetadataReaderContentEncoding|TestUtilsTextEncoding|TestMediaFoundationHeaderSelfContained|TestAudioPlaybackCursor|TestAudioPlaybackInit|TestAudioDataSourceFormat|TestDiagnosticFileLifecycle{PROCESS_LAUNCH_TEST}",
        "platform ledger additions, native-type leakage, and performance-baseline drift",
    ),
]


def selected_modules(module: str | None, all_modules: bool) -> list[BehaviorModule]:
    if all_modules:
        return MODULES
    if module is None:
        return []
    matches = [item for item in MODULES if item.key == module]
    if not matches:
        known = ", ".join(item.key for item in MODULES)
        raise SystemExit(f"Unknown module `{module}`. Known modules: {known}")
    return matches


def list_modules(modules: list[BehaviorModule] | None = None) -> None:
    for item in modules or MODULES:
        print(f"{item.key}")
        print(f"  contract: {item.contract}")
        print(f"  ctest:    {item.ctest_regex}")
        print(f"  diag:     {item.diagnostics}")


def run_command(command: list[str], env: dict[str, str] | None = None) -> int:
    print("Running:", " ".join(command))
    result = subprocess.run(command, cwd=str(ROOT), text=True, env=env)
    return result.returncode


def run_pregates() -> int:
    commands = [
        [PYTHON, str(ROOT / "tools" / "behavior_contract_check.py")],
        [PYTHON, str(ROOT / "tools" / "graphify_auto_refresh.py"), "--once"],
    ]
    for command in commands:
        code = run_command(command)
        if code != 0:
            return code
    return 0


def module_targets(modules: list[BehaviorModule]) -> list[str]:
    ordered: list[str] = []
    seen: set[str] = set()
    for item in modules:
        for name in item.ctest_regex.split("|"):
            if name and name not in seen:
                seen.add(name)
                ordered.append(name)
    return ordered


def build_targets(targets: list[str]) -> int:
    code = run_command(["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR)])
    if code != 0:
        return code
    buildable = [name for name in targets if (BUILD_DIR / f"{name}.vcxproj").exists()]
    if not buildable:
        return 0
    command = [
        "cmake",
        "--build",
        str(BUILD_DIR),
        "--config",
        "Debug",
        "--target",
        *buildable,
        "--",
        "/m:1",
    ]
    return run_command(command)


def executable_missing(test_name: str) -> bool:
    exe_path = BUILD_DIR / "Debug" / f"{test_name}.exe"
    return not exe_path.exists()


def run_registered_ctest(test_name: str, env: dict[str, str] | None = None) -> int:
    command = [
        "ctest",
        "--test-dir",
        str(BUILD_DIR),
        "-C",
        "Debug",
        "-R",
        f"^{test_name}$",
        "--output-on-failure",
    ]
    return run_command(command, env=env)


def run_built_executable(test_name: str, env: dict[str, str] | None = None) -> int:
    exe_path = BUILD_DIR / "Debug" / f"{test_name}.exe"
    if not exe_path.exists():
        print(f"Missing built executable fallback: {exe_path}")
        return 3
    return run_command([str(exe_path)], env=env)


def run_modules(modules: list[BehaviorModule]) -> int:
    if not BUILD_DIR.exists():
        print(f"Build directory missing: {BUILD_DIR}")
        print("Build the Debug test targets before running behavior regression.")
        return 2
    targets = module_targets(modules)
    if any(executable_missing(name) for name in targets):
        code = build_targets(targets)
        if code != 0:
            return code
    known_tests = ctest_names()
    for item in modules:
        names = [name for name in item.ctest_regex.split("|") if name]
        missing = [
            name for name in names
            if name not in known_tests and executable_missing(name)
        ]
        if missing:
            print(f"{item.key}: mapped test target(s) missing from both CTest and built executables: {', '.join(missing)}")
            return 3
        env = None
        if item.key == "cover-replacement-retirement":
            env = os.environ.copy()
            env["AURA_TEST_DATABASE_FEATURES_CASE"] = "cover_tile_membership"
        for name in names:
            code = (
                run_registered_ctest(name, env=env)
                if name in known_tests
                else run_built_executable(name, env=env)
            )
            if code != 0:
                return code
    return 0


def ctest_names() -> set[str]:
    result = subprocess.run(
        ["ctest", "--test-dir", str(BUILD_DIR), "-C", "Debug", "-N"],
        cwd=str(ROOT),
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        return set()
    names: set[str] = set()
    for line in result.stdout.splitlines():
        if ":" not in line or "Test" not in line:
            continue
        name = line.rsplit(":", 1)[-1].strip()
        if name:
            names.add(name)
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="List behavior regression modules")
    parser.add_argument("--module", help="Select one module by key")
    parser.add_argument("--all", action="store_true", help="Select every behavior module")
    parser.add_argument("--run", action="store_true", help="Run pregates and selected ctest regexes")
    args = parser.parse_args()

    modules = selected_modules(args.module, args.all)
    if args.list or not args.run:
        list_modules(modules or None)
    if not args.run:
        return 0
    if not modules:
        print("Use --module <key> or --all with --run.")
        return 2
    code = run_pregates()
    if code != 0:
        return code
    return run_modules(modules)


if __name__ == "__main__":
    raise SystemExit(main())
