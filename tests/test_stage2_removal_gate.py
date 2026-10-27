from __future__ import annotations

import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTER = ROOT / "docs" / "plans" / "refactor" / "stage-2-dead-code-register.json"
TOOL = ROOT / "tools" / "stage2_dead_code_audit.py"


class Stage2RemovalGate(unittest.TestCase):
    def test_reviewed_register_is_closed_against_current_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp = Path(temp_dir)
            register = temp / "register.json"
            report = temp / "report.md"
            shutil.copyfile(REGISTER, register)
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--root",
                    str(ROOT),
                    "--register",
                    str(register),
                    "--output",
                    str(report),
                    "--check-closed",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
        self.assertEqual(
            0,
            result.returncode,
            "Stage 2 仍有已批准删除项或未裁决的新候选：\n"
            + result.stdout
            + result.stderr,
        )

    def test_disconnected_webui_records_are_gone(self) -> None:
        removed_paths = (
            "src/WebUIBridge.cpp",
            "src/WebUIBridge.h",
            "LICENSES/React-MIT.txt",
        )
        remaining = [path for path in removed_paths if (ROOT / path).exists()]
        self.assertEqual([], remaining, f"已批准的 WebUI 死路径仍存在：{remaining}")
        self.assertTrue((ROOT / "src/webui").is_dir(), "active WebUI twin must remain available")

        public_text = "\n".join(
            (ROOT / path).read_text(encoding="utf-8", errors="replace")
            for path in ("README.md", "PRIVACY.md", "THIRD_PARTY_NOTICES.md", "LICENSES/README.md")
        )
        for token in (
            "WebUiEmbedded.generated.h",
            "EMBEDDED_RUNTIME_MANIFEST.md",
            "React-MIT.txt",
            "ReactDOM 19.2.4",
            "scheduler 0.27.0",
        ):
            self.assertNotIn(token, public_text, f"公开发布记录仍声称携带已删除运行时：{token}")

    def test_orphan_stubs_and_helper_are_gone(self) -> None:
        removed_paths = (
            "cmake/CopyIfExists.cmake",
            "src/ThemeEditorStub.cpp",
            "src/ThemeInspectorStub.cpp",
            "src/ThemeMascotStub.cpp",
            "src/WaveformSliceAnalyzer.h",
        )
        remaining = [path for path in removed_paths if (ROOT / path).exists()]
        self.assertEqual([], remaining, f"已批准的死路径仍存在：{remaining}")

    def test_reviewed_disabled_blocks_are_gone(self) -> None:
        findings: list[str] = []
        for relative_path in (
            "src/WindowsMain.cpp",
            "src/WwiseTransferPanel.cpp",
            "src/main.cpp",
        ):
            source = (ROOT / relative_path).read_text(encoding="utf-8", errors="replace")
            findings.extend(
                f"{relative_path}:{line_number}"
                for line_number, line in enumerate(source.splitlines(), start=1)
                if re.match(r"^\s*#\s*if\s+0(?:\s|$)", line)
            )
        self.assertEqual([], findings, f"已批准删除的恒关闭块仍存在：{findings}")

    def test_coverwall_contract_conflict_stays_preserved(self) -> None:
        self.assertTrue(
            (ROOT / "src/coverwall_lab/CoverWallLabMain.cpp").is_file(),
            "CoverWallLab 产品契约尚未裁决，Stage 2 不得删除其入口源码",
        )


if __name__ == "__main__":
    unittest.main()
