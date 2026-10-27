from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
FACTORY = (ROOT / "src" / "AsyncSearchResultEffectFactory.cpp").read_text(encoding="utf-8")
UI_RENDERER = (ROOT / "src" / "UIRenderer.h").read_text(encoding="utf-8")
HEADER_PATH = ROOT / "src" / "PlaybackHistoryStateAdapter.h"
SOURCE_PATH = ROOT / "src" / "PlaybackHistoryStateAdapter.cpp"
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
BEHAVIOR = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(
    encoding="utf-8"
)
RULES = (
    ROOT / "docs" / "architecture" / "stage-4-dependency-rules.json"
).read_text(encoding="utf-8")
STAGE7 = (ROOT / "tools" / "stage7_api_contract_audit.py").read_text(
    encoding="utf-8"
)


def test_playback_history_state_adapter_owns_played_state_and_history():
    assert HEADER_PATH.exists()
    assert SOURCE_PATH.exists()

    header = HEADER_PATH.read_text(encoding="utf-8")
    source = SOURCE_PATH.read_text(encoding="utf-8")

    assert "namespace aura::playback" in header
    assert "void MarkFilePlayed(" in header
    assert "void RecordFilePlayed(" in header
    assert "bool IsFilePlayed(" in header
    assert '#include "AppState.h"' in source
    assert "state.playedFiles.insert(filePath);" in source
    assert "if (fileId > 0)" in source
    assert "state.playedFileIds.insert(fileId);" in source
    assert "if (!addToHistory) return;" in source
    assert 'filePath.find_last_of("/\\\\")' in source
    assert "state.addToPlayHistory(fileId, filePath, fileName);" in source

    for forbidden in ("main.cpp", "WindowsMain", "platform/"):
        assert forbidden not in header
        assert forbidden not in source


def test_global_playback_history_abi_is_preserved_below_root():
    source = SOURCE_PATH.read_text(encoding="utf-8")

    assert (
        "void MarkFilePlayed(AppState& state, const std::string& filePath, "
        "int64_t fileId = 0, bool addToHistory = true);"
        in UI_RENDERER
    )
    assert (
        "bool IsFilePlayed(const AppState& state, const std::string& filePath);"
        in UI_RENDERER
    )
    assert "void MarkFilePlayed(AppState& state," in source
    assert "aura::playback::RecordFilePlayed(" in source
    assert "bool IsFilePlayed(const AppState& state," in source
    assert "return aura::playback::IsFilePlayed(state, filePath);" in source

    assert '#include "PlaybackHistoryStateAdapter.h"' in MAIN
    assert "void MarkFilePlayed(AppState& state," not in MAIN
    assert "bool IsFilePlayed(const AppState& state," not in MAIN
    assert FACTORY.count("MarkFilePlayed,") == 1


def test_playback_history_state_adapter_is_built_tested_and_governed():
    assert CMAKE.count("src/PlaybackHistoryStateAdapter.cpp") == 1
    assert BEHAVIOR.count(
        "tests/test_playback_history_state_adapter_boundary.py"
    ) == 1
    assert RULES.count("src/PlaybackHistoryStateAdapter.cpp") >= 2
    assert RULES.count("src/PlaybackHistoryStateAdapter.h") >= 2
    assert (
        '"src/PlaybackHistoryStateAdapter.h": '
        '"tests/test_playback_history_state_adapter_boundary.py"'
        in STAGE7
    )
