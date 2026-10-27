from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/TagCloudContext.h"
MAIN = ROOT / "src/main.cpp"
BEHAVIOR_CMAKE = ROOT / "cmake/harken_search_behavior_tests.cmake"


def _tag_cloud_block(source: str) -> str:
    start = source.index("if (activeSectionId == 4 && state.settings.showTagCloud)")
    end = source.index("// Section 2:", start)
    return source[start:end]


def test_tag_cloud_context_is_a_pure_owned_identity_model() -> None:
    assert HEADER.is_file(), (
        "TagCloudContext.h is the neutral seam for async tag-cloud identity "
        "and owned results"
    )
    source = HEADER.read_text(encoding="utf-8")

    for required in (
        "namespace aura::tag_cloud",
        "struct TagCloudContextIdentity",
        "std::string databaseIdentity",
        "std::string databasePath",
        "std::string tabId",
        "std::uint64_t searchResultsGeneration",
        "std::uint64_t filterQueryGeneration",
        "struct TagCloudResult",
        "TagCloudContextIdentity identity",
        "std::vector<std::pair<std::string, int>> tags",
        "bool IsCurrent(",
    ):
        assert required in source, required

    for forbidden in (
        "imgui.h",
        "ImGui",
        "AppState.h",
        "AudioEngine.h",
        "Database.h",
        "main.cpp",
        "<future>",
        "std::future",
        "std::async",
        "std::thread",
        "std::atomic",
    ):
        assert forbidden not in source, forbidden


def test_tag_cloud_async_result_checks_full_identity_before_apply() -> None:
    source = MAIN.read_text(encoding="utf-8")
    block = _tag_cloud_block(source)

    assert '#include "TagCloudContext.h"' in source
    for required in (
        "TagCloudContextIdentity",
        "TagCloudResult",
        "databaseIdentity",
        "databasePath",
        "tabId",
        "searchResultsGeneration",
        "filterQueryGeneration",
        "IsCurrent(",
        "std::async(",
        "std::launch::async",
        "std::future_status::ready",
    ):
        assert required in block, required

    identity_check = block.index("IsCurrent(")
    result_apply = block.index("s_topTags =", identity_check)
    assert identity_check < result_apply, (
        "tag-cloud async results must be checked against the complete context "
        "before becoming visible"
    )


def test_tag_cloud_context_boundary_is_registered_once() -> None:
    behavior = BEHAVIOR_CMAKE.read_text(encoding="utf-8")
    assert behavior.count("NAME TestTagCloudContextBoundary") == 1
    assert "tests/test_tag_cloud_context_boundary.py" in behavior
