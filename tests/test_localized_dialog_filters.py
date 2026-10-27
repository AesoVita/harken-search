"""Compile the real filter expressions without opening any native dialogs."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LocalizedDialogFilterTest(unittest.TestCase):
    def test_filters_keep_description_pattern_pairs_and_double_terminator(self):
        code = ['#include <string>', '#include <algorithm>', '#include <iostream>',
                'using namespace std::string_literals;', 'int main() { int failed = 0;']
        count = 0
        for path in sorted((ROOT / "src/platform").glob("*dialog*_win32.cpp")):
            source = path.read_text(encoding="utf-8")
            expressions = re.findall(r'const std::wstring filterBuf\s*=\s*(.*?);\s*ofn.lpstrFilter', source, re.S)
            expressions += re.findall(r'const std::wstring filter\s*=\s*(.*?);\s*return filter;', source, re.S)
            expressions = [expr for expr in expressions if 'Lw(' in expr]
            for expression in expressions:
                pairs = len(re.findall(r'Lw\(', expression))
                expression = re.sub(r'Lw\(\w+\)', 'L"Localized label"', expression)
                code.extend(['{ const std::wstring filterBuf = ' + expression + ';',
                    f'if (std::count(filterBuf.begin(), filterBuf.end(), wchar_t(0)) != {pairs * 2}',
                    '    || filterBuf.empty() || filterBuf.back() != wchar_t(0)) {',
                    f'std::cerr << "Malformed filter: {path.name}" << std::endl; ++failed;', '}}'])
                count += 1
        self.assertGreaterEqual(count, 8)
        code.append('return failed; }')
        with tempfile.TemporaryDirectory(prefix="aris-filter-test-") as tmp:
            tmp = Path(tmp)
            (tmp / "test.cpp").write_text(chr(10).join(code), encoding="utf-8")
            (tmp / "CMakeLists.txt").write_text(chr(10).join([
                'cmake_minimum_required(VERSION 3.20)', 'project(FilterTest LANGUAGES CXX)',
                'add_executable(FilterTest test.cpp)',
                'target_compile_features(FilterTest PRIVATE cxx_std_17)']), encoding="utf-8")
            for command in (["cmake", "-S", str(tmp), "-B", str(tmp / "build"), "-G", "Visual Studio 17 2022", "-A", "x64"],
                            ["cmake", "--build", str(tmp / "build"), "--config", "Release"],
                            [str(tmp / "build/Release/FilterTest.exe")]):
                result = subprocess.run(command, capture_output=True, text=True, errors="replace")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
