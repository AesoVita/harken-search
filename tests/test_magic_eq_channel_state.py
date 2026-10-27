"""Device-free execution of AudioEngine's channel-availability transitions."""

from pathlib import Path
import glob
import shutil
import subprocess

import pytest


ROOT = Path(__file__).parents[1]
CPP = ROOT / "src/AudioEngine.cpp"
MAIN = ROOT / "src/main.cpp"


def _prefix(source: str, signature: str, next_statement: str) -> str:
    return source.split(signature, 1)[1].split(next_statement, 1)[0]


def _compile_and_run(source: str, tmp_path: Path) -> subprocess.CompletedProcess[str]:
    compiler = shutil.which("cl.exe")
    vcvars = None
    if compiler is None:
        candidates = glob.glob(
            "C:/Program Files/Microsoft Visual Studio/*/*/VC/Auxiliary/Build/vcvars64.bat"
        )
        if candidates:
            vcvars = candidates[-1]
    if compiler is None and vcvars is None:
        pytest.skip("MSVC is required for this Windows-only state test")

    source_path = tmp_path / "magic_eq_channel_state.cpp"
    executable = tmp_path / "magic_eq_channel_state.exe"
    source_path.write_text(source, encoding="utf-8")
    compile_command = f'cl /nologo /EHsc /std:c++17 /Fe:"{executable}" "{source_path}"'
    if vcvars:
        compile_command = f'call "{vcvars}" >nul 2>nul\nif errorlevel 1 exit /b 1\n{compile_command}'
    batch_path = tmp_path / "compile_magic_eq_channel_state.cmd"
    batch_path.write_text(f"@echo off\n{compile_command}\n", encoding="ascii")
    compiled = subprocess.run(
        ["cmd.exe", "/d", "/c", str(batch_path)],
        cwd=tmp_path,
        capture_output=True,
        text=True,
    )
    assert compiled.returncode == 0, compiled.stdout + compiled.stderr
    return subprocess.run([str(executable)], capture_output=True, text=True)


def test_enabled_eq_temporarily_bypasses_multichannel_then_recovers(tmp_path):
    cpp = CPP.read_text(encoding="utf-8")
    reset = _prefix(
        cpp, "void AudioEngine::resetState(bool clearVisualWaveform)",
        "if (clearVisualWaveform)",
    )
    availability = _prefix(
        cpp, "void AudioEngine::enforceMagicEQAvailabilityLocked()",
        "if (m_magicEQ.enabled && m_fileInfo.sampleRate > 0)",
    )
    saved_settings = _prefix(
        cpp, "MagicEQSettings AudioEngine::getMagicEQUserSettings() const",
        "AudioEngine::TransferProcessingSnapshot AudioEngine::captureTransferProcessingSnapshot() const",
    )
    source = f"""
#include <cstdint>
#include <mutex>
struct Settings {{ bool enabled = false; bool invalidInput = false; }};
using MagicEQSettings = Settings;
struct FileInfo {{ int sampleRate = 0; }};
using AudioFileInfo = FileInfo;
struct AudioEngine {{
    mutable std::mutex m_mutex;
    FileInfo m_fileInfo;
    std::uint32_t m_playbackChannels = 0;
    bool m_magicEQUnsupportedChannels = false;
    bool m_magicEQUserEnabled = false;
    Settings m_magicEQ;
    void resetState(bool clearVisualWaveform) {reset}
    }}
    void enforceMagicEQAvailabilityLocked() {availability}
    }}
    MagicEQSettings getMagicEQUserSettings() const {saved_settings}
}};
int main() {{
    AudioEngine engine;
    engine.m_magicEQ.enabled = true;
    engine.m_magicEQUserEnabled = true;
    engine.m_playbackChannels = 6;
    engine.enforceMagicEQAvailabilityLocked();
    if (engine.m_magicEQ.enabled || !engine.m_magicEQ.invalidInput) return 1;
    const auto saved = engine.getMagicEQUserSettings();
    if (!saved.enabled || saved.invalidInput) return 4;
    engine.resetState(false);
    engine.m_playbackChannels = 2;
    engine.enforceMagicEQAvailabilityLocked();
    if (!engine.m_magicEQ.enabled || engine.m_magicEQ.invalidInput) return 2;
    engine.resetState(false);
    engine.m_playbackChannels = 1;
    engine.enforceMagicEQAvailabilityLocked();
    if (!engine.m_magicEQ.enabled || engine.m_magicEQ.invalidInput) return 3;
    engine.m_playbackChannels = 6;
    engine.enforceMagicEQAvailabilityLocked();
    engine.m_playbackChannels = 2; // Reverse reload changes source without resetState().
    engine.enforceMagicEQAvailabilityLocked();
    if (!engine.m_magicEQ.enabled || engine.m_magicEQ.invalidInput) return 5;
    return 0;
}}
"""
    run = _compile_and_run(source, tmp_path)
    assert run.returncode == 0, f"state transition failed: {run.returncode}: {run.stdout}{run.stderr}"


def test_settings_save_reads_requested_eq_state():
    main = MAIN.read_text(encoding="utf-8")
    save = main.split("bool TrySaveAppSettings(", 1)[1].split("magicEqEnabled", 1)[0]
    assert "state.audioEngine.getMagicEQUserSettings()" in save
