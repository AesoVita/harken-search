from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/EmbedMetadataBatchRuntime.h"
SOURCE = ROOT / "src/EmbedMetadataBatchRuntime.cpp"
assert HEADER.is_file(), f"missing header: {HEADER}"
assert SOURCE.is_file(), f"missing source: {SOURCE}"

header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")

for token in (
    "struct EmbedMetadataTask",
    "class EmbedMetadataBatchStatePort",
    "class EmbedMetadataTaskPort",
    "class EmbedMetadataBatchRuntime",
    "void pump(",
    "bool inFlight() const",
):
    assert token in header, token

for forbidden in (
    "AppState", "AudioRecord", "Database", "MetadataWriter", "ImGui",
    "Platform::", "platform/", "main.cpp", "WindowsMain",
):
    assert forbidden not in header and forbidden not in source, forbidden

pump = main.split("static void PumpEmbedMetadataBatch(AppState& state)", 1)[1].split(
    "// ", 1
)[0]
assert pump.count("runtime.pump(") == 1
for moved in (
    "static std::future", "static bool", "wait_for(", "future_status::ready",
    "embedBatchProgress++", "embedBatchActive = false",
    "embedBatchTargets.clear()", "getCoverThumb(", "loadImageFile(",
    "writeAlbumArt(", "removeAlbumArt(", "std::async(",
):
    assert moved not in pump, moved

render = main.split("void RenderUI(AppState& state)", 1)[1]
assert render.index("PumpCoverBatchOperation(state);") < render.index(
    "PumpEmbedMetadataBatch(state);"
) < render.index("PumpCoverRebuild(state);")
assert cmake.count("src/EmbedMetadataBatchRuntime.cpp") == 1
assert behavior.count("src/EmbedMetadataBatchRuntime.cpp") == 1
assert "TestEmbedMetadataBatchRuntime" in behavior
assert "NAME TestEmbedMetadataBatchRuntimeBoundary" in behavior

ports = main.split("class AppEmbedMetadataBatchPorts", 1)[1].split("static void PumpEmbedMetadataBatch", 1)[0]
launch = ports.split("std::future<bool> launch", 1)[1]
finish = ports.split("void finish()", 1)[1].split("std::future<bool> launch", 1)[0]
assert "blockedNetwork_" not in ports, "blockedNetwork_ must be deleted"
assert "[this" not in launch, "launch must not capture this"
assert launch.find("IsNetworkPath") < launch.find("std::async"), "network reject must be on UI thread before async"
assert "set_value(false)" in launch, "network reject returns a ready future"
assert "loadErrorToast" not in finish, "finish must not toast from destroyed ports"
assert "embedBatchActive = false" in finish and "embedBatchTargets.clear()" in finish

print("PASS: embed metadata batch runtime boundary contracts")
