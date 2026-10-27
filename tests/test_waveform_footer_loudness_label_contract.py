from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "cmake/harken_search_behavior_tests.cmake").read_text(encoding="utf-8")


def _normalize_block() -> str:
    start = MAIN.index('"##MNormalize"')
    end = MAIN.index("if (HoveredForHelp())", start)
    end = MAIN.index("}", end)
    return MAIN[start:end]


def _icon_siblings() -> tuple[str, str, str]:
    return (
        MAIN[MAIN.index('MiniModeBtn("##MRandom"') : MAIN.index('MiniModeBtn("##MListenMode"')],
        MAIN[MAIN.index('MiniModeBtn("##MListenMode"') : MAIN.index('MiniModeBtn("##MLoop"')],
        MAIN[MAIN.index('MiniModeBtn("##MLoop"') : MAIN.index('"##MNormalize"')],
    )


def test_cmake_registers_the_loudness_label_contract() -> None:
    assert "TestWaveformFooterLoudnessLabelContract" in CMAKE


def test_normalize_button_shows_plain_loudness_text_instead_of_bar_icon() -> None:
    block = _normalize_block()
    assert "AddText" in block
    assert "Loudness" in block
    assert "CalcTextSize" in block
    assert "AddLine" not in block
    assert "cx - 5.0f" not in block
    assert "cx + 4.0f" not in block


def test_shuffle_loop_and_listen_mode_keep_icon_buttons() -> None:
    random_block, listen_block, loop_block = _icon_siblings()
    assert 'MiniModeBtn("##MRandom"' in random_block
    assert 'MiniModeBtn("##MListenMode"' in listen_block
    assert 'MiniModeBtn("##MLoop"' in loop_block
    assert "AddLine" in random_block or "AddTriangleFilled" in random_block
    assert "AddTriangleFilled" in listen_block or "PathArcTo" in listen_block
    assert "PathArcTo" in loop_block
    assert "AddText" not in random_block
    assert "AddText" not in listen_block
    assert "AddText" not in loop_block
