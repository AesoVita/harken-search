import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/AITranscriptionResultStateAdapter.h"
SOURCE = ROOT / "src/AITranscriptionResultStateAdapter.cpp"
MAIN = ROOT / "src/main.cpp"


def test_adapter_owns_completed_transcription_result_state_transition() -> None:
    assert HEADER.is_file(), f"missing header: {HEADER}"
    assert SOURCE.is_file(), f"missing source: {SOURCE}"

    header = HEADER.read_text(encoding="utf-8")
    source = SOURCE.read_text(encoding="utf-8")
    assert "class AITranscriptionResultStateAdapter final" in header
    assert "AITranscriptionResultStateAdapter(AppState& state, AIEngine& engine)" in header
    assert "void pump()" in header
    for include in (
        '#include "AppState.h"',
        '#include "AIEngine.h"',
        '#include "TagSystem.h"',
        '#include "VoiceAssetOrganizer.h"',
    ):
        assert include in source
    for owned in (
        "engine_.GetTask(taskId)",
        "aiTranscriptionTaskGenerations",
        "aiTranscriptionFileGenerations",
        "aiTranscriptCache",
        "aiTranscriptResultCache",
        "database.getFileById(fileId)",
        "VoiceAssetOrganizer::Analyze",
        "TagSystemManager::suggestTagsFromText",
        "aiTranscriptionTaskFileIds.erase(taskId)",
        "aiCurrentTranscriptionTaskId = -1",
    ):
        assert owned in source
    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_product_root_only_composes_the_transcription_result_adapter() -> None:
    main = MAIN.read_text(encoding="utf-8")
    assert '#include "AITranscriptionResultStateAdapter.h"' in main
    body = main.split("static void PumpAITranscriptionResults(AppState& state) {", 1)[
        1
    ].split("\n}", 1)[0]
    assert body.count("AITranscriptionResultStateAdapter") == 1
    assert body.count("adapter.pump();") == 1
    for moved in (
        "GetTask(",
        "aiTranscriptionTaskGenerations",
        "aiTranscriptionFileGenerations",
        "VoiceAssetOrganizer::Analyze",
        "TagSystemManager::suggestTagsFromText",
        "database.getFileById",
        "aiTranscriptionTaskFileIds.erase",
    ):
        assert moved not in body


def test_adapter_is_built_and_governed_as_composition() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    behavior = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
    rules = json.loads(
        (ROOT / "docs/architecture/stage-4-dependency-rules.json").read_text(
            encoding="utf-8"
        )
    )
    stage7 = (ROOT / "tools/stage7_api_contract_audit.py").read_text(
        encoding="utf-8"
    )

    assert "src/AITranscriptionResultStateAdapter.cpp" in cmake
    assert "TestAITranscriptionResultStateAdapterBoundary" in behavior
    for path in (
        "src/AITranscriptionResultStateAdapter.cpp",
        "src/AITranscriptionResultStateAdapter.h",
    ):
        assert path in rules["scope"]
        assert path in rules["modules"]["composition"]["files"]
    assert (
        '"src/AITranscriptionResultStateAdapter.h": '
        '"tests/test_ai_transcription_result_state_adapter_boundary.py"'
    ) in stage7
