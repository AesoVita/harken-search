from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/CoverRebuildRecordProcessor.h"
SOURCE = ROOT / "src/CoverRebuildRecordProcessor.cpp"
assert HEADER.is_file(), f"missing header: {HEADER}"
assert SOURCE.is_file(), f"missing source: {SOURCE}"

header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")

for token in (
    "struct CoverRebuildRecord",
    "class CoverRebuildFolderLookupPort",
    "struct CoverRebuildRecordResult",
    "ProcessCoverRebuildRecord(",
):
    assert token in header, token

for forbidden in (
    "AppState", "AudioRecord", "Database", "CoverModule", "ImGui",
    "main.cpp", "WindowsMain", "platform/",
):
    assert forbidden not in header and forbidden not in source, forbidden

pump = main.split("static void PumpCoverRebuild(AppState& state)", 1)[1].split(
    "// --- Rebuild all waveform caches", 1
)[0]
assert pump.count("aura::cover_rebuild::ProcessCoverRebuildRecord(") == 1
for moved in (
    "TryGetFolderCoverKeyFast(", "GetFolderCoverImagePath(",
    "NormalizeCoverKeyByVisualIdentity(", "hasEmbeddedCover =",
    "hasAnyCover =", "coverRebuildFound++",
):
    assert moved not in pump, moved
print("PASS: cover rebuild record processor boundary contracts")
