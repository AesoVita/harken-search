import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests" / "test_stage6_audio_device_probe.cpp"
CMAKE = ROOT / "cmake" / "harken_search_behavior_tests.cmake"


class Stage6AudioDeviceProbeContractTests(unittest.TestCase):
    def test_probe_source_and_focused_target_exist(self):
        self.assertTrue(SOURCE.exists(), SOURCE)
        source = SOURCE.read_text(encoding="utf-8")
        cmake = CMAKE.read_text(encoding="utf-8")
        self.assertIn("add_executable(TestStage6AudioDeviceProbe", cmake)
        self.assertIn("tests/test_stage6_audio_device_probe.cpp", cmake)
        self.assertIn("kTargetCallbacks = 250", source)

    def test_callback_is_realtime_safe_and_output_contract_is_machine_readable(self):
        source = SOURCE.read_text(encoding="utf-8")
        callback = re.search(
            r"static void AudioCallback\(.*?\)\s*\{(?P<body>.*?)\n\}",
            source,
            re.S,
        )
        self.assertIsNotNone(callback)
        body = callback.group("body")
        for forbidden in ("std::cout", "printf(", "new ", "malloc(", "mutex", "ofstream", "fopen("):
            self.assertNotIn(forbidden, body)
        for field in ("status", "period_ms", "callback_execution", "median_ms", "p95_ms",
                      "max_ms", "inter_arrival", "p95_jitter_ms", "callback_count"):
            self.assertIn(f'\\"{field}\\"', source)
        self.assertIn('\\"environment_gate\\"', source)
        self.assertIn("return 2;", source)


if __name__ == "__main__":
    unittest.main()
