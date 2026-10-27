import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_STATE = (ROOT / "src" / "AppState.h").read_text(encoding="utf-8")
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR_CMAKE = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


class AppStateNativeIncludeContract(unittest.TestCase):
    def test_header_has_no_native_window_or_gl_include(self):
        self.assertNotIn("#include <windows.h>", APP_STATE)
        self.assertNotIn("#include <GL/gl.h>", APP_STATE)
        for native_type in (
            "HWND", "HANDLE", "HINSTANCE", "HDC", "DWORD",
            "WPARAM", "LPARAM", "LRESULT",
        ):
            self.assertIsNone(re.search(rf"\b{native_type}\b", APP_STATE))

    def test_gluint_abi_and_member_remain_explicit(self):
        self.assertIn("typedef unsigned int GLuint;", APP_STATE)
        self.assertIn("GLuint spectrogramTexture = 0;", APP_STATE)

    def test_macro_neutralization_contract_remains(self):
        for token in (
            "#ifndef NOMINMAX\n#define NOMINMAX\n#endif",
            "#ifdef SetWindowPos\n#undef SetWindowPos\n#endif",
            "#ifdef GetWindowPos\n#undef GetWindowPos\n#endif",
        ):
            self.assertIn(token, APP_STATE)

    def test_contract_and_compile_fixture_are_registered(self):
        self.assertIn("TestAppStateNativeIncludeContract", CMAKE)
        self.assertIn("tests/test_app_state_native_include_contract.py", CMAKE)
        self.assertIn("TestAppStateHeaderNativeFree", BEHAVIOR_CMAKE)
        self.assertIn("tests/test_app_state_header_native_free.cpp", BEHAVIOR_CMAKE)


if __name__ == "__main__":
    unittest.main()
