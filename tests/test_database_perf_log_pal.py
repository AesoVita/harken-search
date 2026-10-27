import pathlib
import unittest


class DatabasePerfLogPalContractTests(unittest.TestCase):
    def test_cover_perf_log_uses_pal_safe_temp_append(self):
        source = (pathlib.Path(__file__).resolve().parents[1] / "src" / "Database.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("AppendCoverPerfLog", source, "Database cover perf helper must exist")
        self.assertIn(
            "Platform::FileSystem::TemporaryDirectory()",
            source,
            "Database cover perf helper must use a platform temp directory path",
        )
        self.assertNotIn(
            'C:\\\\tmp\\\\auro_perf.log',
            source,
            "Database cover perf helper must not hardcode a Windows temp path",
        )
        self.assertNotIn(
            "fopen_s(&logFile",
            source,
            "Database cover perf helper must not use MSVC-specific fopen_s",
        )
        self.assertNotIn(
            'fopen("C:\\\\tmp\\\\auro_perf.log"',
            source,
            "Database cover perf helper must not use CRT fopen on the hardcoded path",
        )


if __name__ == "__main__":
    unittest.main()
