"""Contract: AI cover generation must not clobber an existing cover.jpg."""

from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "src" / "AIEngine.cpp"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")


def extract_between(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start + len(start_marker))
    return source[start:end]


def cover_path_selection_source(source: str) -> str:
    generate = extract_between(
        source,
        "int AIEngine::GenerateCoverAsync",
        "std::vector<int> AIEngine::BatchTranscribe",
    )
    chunks = [generate]
    helper_start = source.find("ChooseUnusedCoverJpegPath")
    if helper_start < 0:
        helper_start = source.find("SelectCoverOutputPath")
    if helper_start >= 0:
        chunks.append(source[helper_start : source.index("int AIEngine::GenerateCoverAsync")])
    return "\n".join(chunks)


class AICoverPathProtectionContract(unittest.TestCase):
    def test_generate_cover_does_not_unconditionally_use_cover_jpg(self) -> None:
        generate = extract_between(
            SOURCE,
            "int AIEngine::GenerateCoverAsync",
            "std::vector<int> AIEngine::BatchTranscribe",
        )
        self.assertNotRegex(
            generate,
            r'outputPath\s*=\s*targetFolder\s*\+\s*sep\s*\+\s*"cover\.jpg"',
            "GenerateCoverAsync still concatenates cover.jpg without probing",
        )

    def test_generate_cover_probes_missing_present_error(self) -> None:
        selection = cover_path_selection_source(SOURCE)
        self.assertIn("Platform::FileSystem::ProbePath", selection)
        self.assertIn("Platform::FileSystem::PathStatus::Missing", selection)
        self.assertIn("Platform::FileSystem::PathStatus::Present", selection)
        self.assertIn("Platform::FileSystem::PathStatus::Error", selection)
        self.assertIn('"cover.jpg"', selection)
        self.assertTrue(
            '"cover-2.jpg"' in selection
            or ('"cover-"' in selection and "std::to_string" in selection),
            "collision names must be cover-2.jpg, cover-3.jpg, ...",
        )
        self.assertIn(
            "AITaskStatus::Failed",
            extract_between(
                SOURCE,
                "int AIEngine::GenerateCoverAsync",
                "std::vector<int> AIEngine::BatchTranscribe",
            ),
        )

    def test_download_image_writes_part_then_copy_without_clobber(self) -> None:
        download = extract_between(
            SOURCE,
            "static bool DownloadImageToDisk",
            "std::string AIEngine::GetModelUrl",
        )
        self.assertIn("Platform::FileSystem::ProbePath", download)
        self.assertIn("Platform::FileSystem::PathStatus::Missing", download)
        self.assertIn('savePath + ".part"', download)
        self.assertIn("Platform::FileSystem::CopyFile(", download)
        self.assertRegex(download, r"CopyFile\([^;]*false")
        self.assertIn("Platform::FileSystem::RemoveFile", download)
        self.assertNotIn("AtomicFileReplace", download)
        self.assertNotRegex(
            download,
            r"writer\.Open\(\s*savePath\s*,",
            "must not truncate the destination path",
        )
        self.assertNotIn("MessageBox", download)
        self.assertNotIn("AtomicFileReplace", SOURCE[SOURCE.index("static bool DownloadImageToDisk") : SOURCE.index("int AIEngine::GenerateCoverAsync")])


if __name__ == "__main__":
    unittest.main()
