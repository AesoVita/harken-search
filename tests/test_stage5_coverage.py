import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
import sys
import json


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import stage5_coverage  # noqa: E402


class Stage5CoverageTests(unittest.TestCase):
    def _write_report(self, root: Path, classes: list[tuple[Path, list[tuple[int, int]]]]) -> Path:
        coverage = ET.Element("coverage")
        packages = ET.SubElement(coverage, "packages")
        for index, (source, lines) in enumerate(classes):
            package = ET.SubElement(packages, "package", name=f"module-{index}")
            class_nodes = ET.SubElement(ET.SubElement(package, "classes"), "class", filename=str(source))
            line_nodes = ET.SubElement(class_nodes, "lines")
            for number, hits in lines:
                ET.SubElement(line_nodes, "line", number=str(number), hits=str(hits))
        report = root / "coverage.xml"
        ET.ElementTree(coverage).write(report, encoding="utf-8", xml_declaration=True)
        return report

    def test_parse_cobertura_unions_duplicate_production_lines_and_excludes_tests(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "src" / "Database.cpp"
            test_source = root / "tests" / "test_database.cpp"
            source.parent.mkdir()
            test_source.parent.mkdir()
            source.write_text("one\ntwo\nthree\n", encoding="utf-8")
            test_source.write_text("test\n", encoding="utf-8")
            report = self._write_report(
                root,
                [
                    (source, [(1, 1), (2, 0)]),
                    (source, [(2, 3), (3, 0)]),
                    (test_source, [(1, 1)]),
                ],
            )

            summary = stage5_coverage.parse_cobertura(report, root)

            self.assertEqual(summary["files_valid"], 1)
            self.assertEqual(summary["lines_valid"], 3)
            self.assertEqual(summary["lines_covered"], 2)
            self.assertAlmostEqual(summary["line_rate"], 2 / 3)
            self.assertEqual(summary["source_files"], ["src/Database.cpp"])

    def test_parse_cobertura_rejects_report_without_production_source_lines(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            test_source = root / "tests" / "test_only.cpp"
            test_source.parent.mkdir()
            test_source.write_text("test\n", encoding="utf-8")
            report = self._write_report(root, [(test_source, [(1, 1)])])

            with self.assertRaisesRegex(ValueError, "no production source lines"):
                stage5_coverage.parse_cobertura(report, root)

    def test_collect_command_runs_all_ctests_and_requests_cobertura(self):
        command = stage5_coverage.build_collect_command(
            coverage_console=Path("coverage-console.exe"),
            ctest=Path("ctest.exe"),
            build_dir=Path("build_dbg"),
            config="Debug",
            report=Path("coverage.xml"),
            log=Path("coverage.log"),
            jobs=1,
        )

        self.assertEqual(command[:3], ["coverage-console.exe", "collect", "ctest.exe"])
        self.assertIn("--no-tests=error", command)
        self.assertNotIn("-R", command)
        self.assertEqual(command[command.index("-f") + 1], "cobertura")
        self.assertEqual(command[command.index("-j") + 1], "1")

    def test_parse_only_preserves_original_collection_command(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "src" / "main.cpp"
            source.parent.mkdir()
            source.write_text("line\n", encoding="utf-8")
            output = root / "coverage-out"
            output.mkdir()
            generated = self._write_report(root, [(source, [(1, 1)])])
            report = output / "stage5-coverage.cobertura.xml"
            generated.replace(report)
            summary_path = output / "stage5-coverage.summary.json"
            summary_path.write_text(
                json.dumps(
                    {
                        "generated_at": "2026-07-11T00:51:26+08:00",
                        "command": ["coverage-console.exe", "collect", "ctest.exe"],
                        "ctest_scope": "all registered tests",
                    }
                ),
                encoding="utf-8",
            )

            exit_code = stage5_coverage.main(
                [
                    "--root",
                    str(root),
                    "--build-dir",
                    "build_dbg",
                    "--output-dir",
                    str(output),
                    "--parse-only",
                ]
            )
            rewritten = json.loads(summary_path.read_text(encoding="utf-8"))

            self.assertEqual(exit_code, 0)
            self.assertEqual(
                rewritten["command"],
                ["coverage-console.exe", "collect", "ctest.exe"],
            )
            self.assertEqual(rewritten["collected_at"], "2026-07-11T00:51:26+08:00")


if __name__ == "__main__":
    unittest.main()
