"""Release builds must not expose or link the retired pitch-preserving speed path."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_product_target_does_not_compile_retired_processor():
    cmake = read("CMakeLists.txt")
    product_sources = cmake.split("set(APP_SOURCES", 1)[1].split("\n)", 1)[0]
    assert "src/TimeStretch.cpp" not in product_sources


def test_product_runtime_has_no_old_speed_entry_or_processor():
    for path in ("src/AudioEngine.h", "src/AudioEngine.cpp",
                 "src/CenterSenseDataSource.h", "src/CenterSenseDataSource.cpp",
                 "src/AppState.h", "src/AppState.cpp", "src/main.cpp"):
        source = read(path)
        for old_entry in ("TimeStretch", "timeStretch", "setTempo(",
                          "getTempo(", "TempoUp", "TempoDown", "TempoReset"):
            assert old_entry not in source, f"{path} still contains {old_entry}"

def test_pitch_frequency_and_music_tempo_remain_available():
    engine = read("src/AudioEngine.h")
    main = read("src/main.cpp")
    assert "setPitch(float semitones)" in engine
    assert "setFreqShift(float hz)" in engine
    assert "TapTempo" in main
    assert '"tempo",       "Tempo"' in main
