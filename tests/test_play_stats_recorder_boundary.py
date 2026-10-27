from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADAPTER_H = (ROOT / "src" / "PlaybackHistoryStateAdapter.h").read_text(
    encoding="utf-8"
)
ADAPTER_CPP = (ROOT / "src" / "PlaybackHistoryStateAdapter.cpp").read_text(
    encoding="utf-8"
)
WINDOWS_MAIN = (ROOT / "src" / "WindowsMain.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _extract_if_block(source: str, condition_snippet: str) -> str:
    hit = source.find(condition_snippet)
    assert hit != -1, condition_snippet
    open_brace = source.find("{", hit)
    assert open_brace != -1
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace : index + 1]
    raise AssertionError("unbalanced block for " + condition_snippet)


def test_mark_file_played_does_not_write_play_stats_columns():
    assert "incrementPlayCount" not in ADAPTER_CPP
    assert "updateLastPlayed" not in ADAPTER_CPP
    mark_body = ADAPTER_CPP.split("void MarkFilePlayed(", 1)[1].split("{", 1)[1]
    mark_body = mark_body.split("bool IsFilePlayed", 1)[0]
    assert "incrementPlayCount" not in mark_body
    assert "updateLastPlayed" not in mark_body


def test_successful_pump_play_path_records_play_stats_after_play_if_load_ready():
    pump_block = _extract_if_block(
        WINDOWS_MAIN, "state.audioEngine.pumpLoadFileIfPending()"
    )
    play_pos = pump_block.find("state.audioEngine.playIfLoadReady();")
    record_pos = pump_block.find("RecordSuccessfulPlayStats(")
    assert play_pos != -1
    assert record_pos != -1
    assert play_pos < record_pos
    else_block = _extract_if_block(
        WINDOWS_MAIN, "std::string loadErr = state.audioEngine.getLastLoadError();"
    )
    assert "RecordSuccessfulPlayStats(" not in else_block
    assert "incrementPlayCount" not in else_block
    assert "updateLastPlayed" not in else_block


def test_play_stats_recorder_is_owned_by_playback_history_adapter():
    assert "struct PlayStatsEvent" in ADAPTER_H
    assert "struct PlayStatsGate" in ADAPTER_H
    assert "RecordSuccessfulPlayStats(" in ADAPTER_H
    assert "TestPlayStatsRecorder" in CMAKE
    assert "tests/test_play_stats_recorder.cpp" in CMAKE
    assert "tests/test_play_stats_recorder_boundary.py" in CMAKE
