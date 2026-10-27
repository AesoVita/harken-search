"""Resolve both supported SDK layouts using synthetic files only."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PtslSdkLayoutTest(unittest.TestCase):
    def check_layout(self, nested):
        with tempfile.TemporaryDirectory(prefix="aris-ptsl-test-") as tmp:
            root = Path(tmp) / "sdk"
            header = root / "include/PTSLC_CPP/CppPTSLClient.h"
            lib = root / ("lib/PTSLC_CPP.lib" if nested else "PTSLC_CPP.lib")
            dll = root / ("bin/PTSLC_CPP.dll" if nested else "PTSLC_CPP.dll")
            for path in (header, lib, dll):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")
            script = Path(tmp) / "check.cmake"
            script.write_text(chr(10).join([
                f'include("{ROOT.as_posix()}/cmake/ptsl_sdk_layout.cmake")',
                f'aris_resolve_ptsl_sdk("{root.as_posix()}" inc lib dll)',
                f'if(NOT lib STREQUAL "{lib.as_posix()}" OR NOT dll STREQUAL "{dll.as_posix()}")',
                'message(FATAL_ERROR "SDK layout resolved incorrectly")',
                'endif()', '']), encoding="utf-8")
            run = subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_flat_drop(self):
        self.check_layout(False)

    def test_include_lib_bin_sdk(self):
        self.check_layout(True)


if __name__ == "__main__":
    unittest.main()
