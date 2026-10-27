import pathlib
import unittest


class AsyncWorkerPerfLogPalContractTests(unittest.TestCase):
    def test_db_query_perf_log_uses_platform_temp_directory(self):
        source = (pathlib.Path(__file__).resolve().parents[1] / "src" / "AsyncWorker.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("AppendDbQueryPerfLog", source, "AsyncWorker DB query perf helper must exist")
        self.assertIn(
            "Platform::FileSystem::TemporaryDirectory()",
            source,
            "AsyncWorker DB query perf helper must use the platform temp directory",
        )
        self.assertNotIn(
            'C:\\\\tmp\\\\auro_perf.log',
            source,
            "AsyncWorker DB query perf helper must not hardcode a Windows temp path",
        )
        self.assertNotIn(
            "fopen_s(&logFile",
            source,
            "AsyncWorker DB query perf helper must not use MSVC-specific fopen_s",
        )


if __name__ == "__main__":
    unittest.main()
