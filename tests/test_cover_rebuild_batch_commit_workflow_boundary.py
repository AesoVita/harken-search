from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/CoverRebuildBatchCommitWorkflow.h"
SOURCE = ROOT / "src/CoverRebuildBatchCommitWorkflow.cpp"
assert HEADER.is_file(), f"missing header: {HEADER}"
assert SOURCE.is_file(), f"missing source: {SOURCE}"
header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
for token in (
    "struct CoverRebuildBatchCommitInput",
    "class CoverRebuildBatchPersistencePort",
    "CommitCoverRebuildBatch(",
):
    assert token in header, token
for forbidden in ("AppState", "Database", "AudioRecord", "CoverModule", "ImGui", "main.cpp", "WindowsMain", "platform/"):
    assert forbidden not in header and forbidden not in source, forbidden
pump = main.split("static void PumpCoverRebuild(AppState& state)", 1)[1].split(
    "// --- Rebuild all waveform caches", 1
)[0]
assert pump.count("aura::cover_rebuild::CommitCoverRebuildBatch(") == 1
for moved in ("updateHasCoverByPathsBatch(", "updateCoverKeyCacheBatch("):
    assert moved not in pump, moved
print("PASS: cover rebuild batch commit workflow boundary contracts")
