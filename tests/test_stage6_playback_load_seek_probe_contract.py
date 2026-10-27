import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "tests/test_stage6_playback_load_seek_probe.cpp"
CMAKE_PATH = ROOT / "cmake/harken_search_behavior_tests.cmake"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8") if SOURCE_PATH.is_file() else ""
CMAKE = CMAKE_PATH.read_text(encoding="utf-8") if CMAKE_PATH.is_file() else ""
DATABASE_STUBS = (ROOT / "tests/test_database_link_stubs.cpp").read_text(encoding="utf-8")


class PlaybackLoadSeekProbeContractTests(unittest.TestCase):
    def test_source_and_focused_target(self):
        self.assertTrue(SOURCE_PATH.is_file())
        self.assertRegex(CMAKE, r"(?s)add_executable\(TestStage6PlaybackLoadSeekProbe\s+EXCLUDE_FROM_ALL.*?test_stage6_playback_load_seek_probe\.cpp")
        self.assertIn("add_test(NAME TestStage6PlaybackLoadSeekProbe", CMAKE)

    def test_target_links_current_platform_dependencies(self):
        for target_name in (
            "TestStage6PlaybackLoadSeekProbe",
            "TestStage6ScanTerminalThroughputProbe",
            "TestSingleChannelExport",
        ):
            target = CMAKE.split(f"add_executable({target_name}", 1)[1].split(
                f"add_test(NAME {target_name}", 1
            )[0]
            for source in (
                "src/NativeFloatReader.cpp",
                "src/platform/audio_data_source_miniaudio.cpp",
                "src/platform/filesystem_win32.cpp",
            ):
                self.assertIn(source, target)
            self.assertNotIn("src/platform/system_win32.cpp", target)
        self.assertIn("EnvironmentStringResult ReadEnvironmentVariable", DATABASE_STUBS)

    def test_public_path_without_decoder_or_play(self):
        for call in (".loadFileAsync(", ".pumpLoadFileIfPending(", ".seekToPosition(", ".getPlaybackPosition("):
            self.assertIn(call, SOURCE)
        self.assertNotIn("ma_decoder_", SOURCE)
        self.assertNotIn(".play(", SOURCE)

    def test_fixed_fixture_workload_and_schema(self):
        tokens = ('"RIFF"', '"WAVEfmt "', "pcm_s16_wav", "48000", "10.0", "480000",
                  "20", "100", "0.10", "0.90", "0.25", "0.75", "0.40", "0.60",
                  "0.05", "0.95", "0.33", "0.67", "schema", "measured", "environment_gate",
                  "failed", "load_to_ready", "seek_call", "poll_interval_ms", "max_position_error", "value_ms")
        for token in tokens:
            self.assertIn(token, SOURCE)
        for declaration in (r"kLoads\s*=\s*20", r"kSeeks\s*=\s*100",
                            r"kPollIntervalMs\s*=\s*1", r"kPerLoadTimeoutSeconds\s*=\s*5",
                            r"kProbeDeadlineSeconds\s*=\s*45"):
            self.assertRegex(SOURCE, declaration)
        self.assertRegex(SOURCE, r"return\s*\{[12],")
        self.assertIn("(std::max)(load.p95, seek.p95)", SOURCE)
        self.assertIn("std::cout << result.json", SOURCE)
        self.assertNotIn("printf(", SOURCE)

    def test_windows_poll_interval_uses_scoped_timer_resolution(self):
        self.assertIn("timeBeginPeriod(kPollIntervalMs)", SOURCE)
        self.assertIn("timeEndPeriod(kPollIntervalMs)", SOURCE)
        self.assertIn("ScopedTimerResolution timerResolution;", SOURCE)
        target = CMAKE.split("add_executable(TestStage6PlaybackLoadSeekProbe", 1)[1].split(
            "add_test(NAME TestStage6PlaybackLoadSeekProbe", 1
        )[0]
        self.assertIn("winmm", target)


if __name__ == "__main__":
    unittest.main()
