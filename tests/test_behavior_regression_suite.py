import importlib
import os
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import behavior_regression_suite  # noqa: E402


class BehaviorRegressionSuiteTests(unittest.TestCase):
    def test_every_module_has_existing_contract_and_ctest_regex(self):
        self.assertGreaterEqual(len(behavior_regression_suite.MODULES), 10)
        for module in behavior_regression_suite.MODULES:
            self.assertTrue((ROOT / module.contract).exists(), module.contract)
            self.assertTrue(module.ctest_regex)
            self.assertNotIn("TBD", module.ctest_regex)

    def test_selects_one_module_or_all_modules(self):
        selected = behavior_regression_suite.selected_modules("search-query", False)
        self.assertEqual([module.key for module in selected], ["search-query"])

        all_selected = behavior_regression_suite.selected_modules(None, True)
        self.assertEqual(len(all_selected), len(behavior_regression_suite.MODULES))

    def test_playlist_mapping_uses_actual_ctest_target_casing(self):
        playlist = behavior_regression_suite.selected_modules("playlist-organization", False)[0]
        self.assertIn("TestPlaylistQuickBarMotion", playlist.ctest_regex)
        self.assertNotIn("TestPlaylistQuickbarMotion", playlist.ctest_regex)

    def test_scan_mapping_includes_pause_resume_contracts(self):
        scan = behavior_regression_suite.selected_modules("scan-indexing", False)[0]
        self.assertIn("TestScanPauseResume", scan.ctest_regex)
        self.assertIn("TestScanPauseResumeContract", scan.ctest_regex)

    def test_build_dir_defaults_to_worktree_debug_build(self):
        previous = os.environ.pop("ARIS_BUILD_DIR", None)
        try:
            importlib.reload(behavior_regression_suite)
            self.assertEqual((ROOT / "build_dbg").resolve(), behavior_regression_suite.BUILD_DIR)
        finally:
            if previous is not None:
                os.environ["ARIS_BUILD_DIR"] = previous
            importlib.reload(behavior_regression_suite)

    def test_build_dir_can_follow_active_worktree_environment(self):
        previous = os.environ.get("ARIS_BUILD_DIR")
        with tempfile.TemporaryDirectory() as temp_dir:
            expected = Path(temp_dir).resolve()
            try:
                os.environ["ARIS_BUILD_DIR"] = temp_dir
                importlib.reload(behavior_regression_suite)
                self.assertEqual(expected, behavior_regression_suite.BUILD_DIR)
            finally:
                if previous is None:
                    os.environ.pop("ARIS_BUILD_DIR", None)
                else:
                    os.environ["ARIS_BUILD_DIR"] = previous
                importlib.reload(behavior_regression_suite)

    def test_platform_mapping_includes_source_only_performance_gate(self):
        platform = behavior_regression_suite.selected_modules("platform-services", False)[0]
        self.assertIn("TestStage6PerformanceBaseline", platform.ctest_regex)
        self.assertIn("TestAudioPlaybackCursor", platform.ctest_regex)
        self.assertIn("TestAudioPlaybackInit", platform.ctest_regex)
        self.assertIn("TestAudioDataSourceFormat", platform.ctest_regex)
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("NAME TestStage6PerformanceBaseline", cmake)
        self.assertIn("stage-6-performance-baseline.json", cmake)

    def test_editor_handoff_maps_installer_only_on_windows(self):
        editor = behavior_regression_suite.selected_modules("editor-handoff", False)[0]
        self.assertEqual(
            "TestDawBridgeInstaller" in editor.ctest_regex,
            os.name == "nt",
        )

    def test_platform_process_launch_mapping_is_windows_only(self):
        platform = behavior_regression_suite.selected_modules("platform-services", False)[0]
        self.assertEqual(
            "TestPlatformProcessLaunch" in platform.ctest_regex,
            os.name == "nt",
        )
        source = (ROOT / "tools" / "behavior_regression_suite.py").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            'PROCESS_LAUNCH_TEST = "|TestPlatformProcessLaunch" if os.name == "nt" else ""',
            source,
        )

    def test_platform_services_runs_diagnostic_file_lifecycle(self):
        platform = behavior_regression_suite.selected_modules("platform-services", False)[0]
        self.assertIn("TestDiagnosticFileLifecycle", platform.ctest_regex)
        cmake = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
            encoding="utf-8"
        )
        target = cmake.split("add_executable(TestDiagnosticFileLifecycle", 1)[1].split(
            ")", 1
        )[0]
        self.assertNotIn("EXCLUDE_FROM_ALL", target)

    def test_build_refreshes_cmake_graph_before_project_filter(self):
        source = (ROOT / "tools" / "behavior_regression_suite.py").read_text(
            encoding="utf-8"
        )
        configure = source.index('["cmake", "-S", str(ROOT), "-B", str(BUILD_DIR)]')
        project_filter = source.index('BUILD_DIR / f"{name}.vcxproj"')
        self.assertLess(configure, project_filter)


if __name__ == "__main__":
    unittest.main()
