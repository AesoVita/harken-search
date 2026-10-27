import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests" / "test_stage6_file_watcher_probe.cpp"
CMAKE = ROOT / "cmake" / "harken_search_behavior_tests.cmake"


class Stage6FileWatcherProbeContractTests(unittest.TestCase):
    def test_probe_source_and_focused_target_exist(self):
        self.assertTrue(SOURCE.exists(), SOURCE)
        source = SOURCE.read_text(encoding="utf-8")
        cmake = CMAKE.read_text(encoding="utf-8")
        self.assertIn("add_executable(TestStage6FileWatcherProbe", cmake)
        block = cmake.split("add_executable(TestStage6FileWatcherProbe", 1)[1].split(
            "add_executable(TestStage6PlaybackLoadSeekProbe", 1
        )[0]
        self.assertIn("${AURO_AUDIO_TEXT_SOURCE}", block)
        self.assertIn("tests/test_stage6_file_watcher_probe.cpp", cmake)
        self.assertIn("kCycles = 50", source)
        self.assertIn("watcher.addWatchDirectory", source)
        self.assertIn("watcher.start()", source)
        self.assertIn("watcher.stop()", source)

    def test_output_and_sequential_wait_contracts_are_present(self):
        source = SOURCE.read_text(encoding="utf-8")
        for field in ("status", "delivery", "median_ms", "p95_ms", "max_ms",
                      "received_count", "event_order", "shutdown_elapsed_ms"):
            self.assertIn(f'\\"{field}\\"', source)
        self.assertIn('\\"environment_gate\\"', source)
        self.assertIn("return 2;", source)
        loop = re.search(r"for \(int cycle = 0; cycle < kCycles; \+\+cycle\).*?\n    \}", source, re.S)
        self.assertIsNotNone(loop)
        for operation in ("Create", "Rename", "Delete"):
            self.assertIn(f"WaitFor{operation}", loop.group(0))


if __name__ == "__main__":
    unittest.main()
