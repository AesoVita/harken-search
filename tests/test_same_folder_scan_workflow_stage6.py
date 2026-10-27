from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_same_folder_scan_pump_delegates_bounded_drain() -> None:
    main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
    workflow = (ROOT / "src/SameFolderScanWorkflow.cpp").read_text(
        encoding="utf-8"
    )
    assert "constexpr int kMaxChunksPerFrame = 4;" in workflow
    assert "constexpr std::size_t kMaxItemsPerFrame = 128;" in workflow
    assert "static void PumpSameFolderScan(" not in main
    assert "g_sameFolderWorkflow" not in main
    assert "g_sameFolderScanner.tryPopChunk(" not in main
