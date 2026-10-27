import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "src/frontend/AuditionFrontendContract.h"
CONTROLLER_H = ROOT / "src/frontend/AuditionFrontendController.h"
CONTROLLER_CPP = ROOT / "src/frontend/AuditionFrontendController.cpp"
ADAPTER_H = ROOT / "src/frontend/AppStateAuditionFrontendAdapter.h"
ADAPTER_CPP = ROOT / "src/frontend/AppStateAuditionFrontendAdapter.cpp"
VIEW_H = ROOT / "src/ui/AuditionTransportView.h"
VIEW_CPP = ROOT / "src/ui/AuditionTransportView.cpp"
MAIN = ROOT / "src/main.cpp"
CMAKE = ROOT / "CMakeLists.txt"
BEHAVIOR_CMAKE = ROOT / "cmake/harken_search_behavior_tests.cmake"


def source(path: Path) -> str:
    assert path.is_file(), f"missing required frontend boundary file: {path}"
    return path.read_text(encoding="utf-8")


def include_targets(text: str) -> set[str]:
    return set(re.findall(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]', text, re.MULTILINE))


def test_contract_and_controller_are_renderer_and_application_neutral() -> None:
    forbidden_includes = {
        "imgui.h",
        "AppState.h",
        "AudioEngine.h",
        "windows.h",
        "filesystem",
        "nlohmann/json.hpp",
    }
    for path in (CONTRACT, CONTROLLER_H, CONTROLLER_CPP):
        text = source(path)
        assert include_targets(text).isdisjoint(forbidden_includes), path
        for forbidden_token in ("WebView2", "rapidjson", "nlohmann::json"):
            assert forbidden_token not in text, (path, forbidden_token)


def test_contract_has_fixed_control_state_without_native_media_payloads() -> None:
    contract = source(CONTRACT)
    for required in (
        "AuditionStateSnapshot",
        "AuditionAction",
        "AuditionDispatchResult",
        "AuditionApplicationPort",
        "ToggleVisibleFooterPlayback",
        "SetVolume",
        "SetPitchControlValue",
        "ChangePitchModeAndReset",
        "ToggleReversePreservingRegion",
        "SetLoopAndRearmRegion",
    ):
        assert required in contract
    for forbidden in (
        "std::vector",
        "std::map",
        "WaveformData",
        "Spectrogram",
        "meterPeaks",
        "meterRms",
    ):
        assert forbidden not in contract


def test_unknown_length_loop_restarts_without_second_click() -> None:
    engine_h = source(ROOT / "src/AudioEngine.h")
    engine = source(ROOT / "src/AudioEngine.cpp")

    pump_fn = engine[
        engine.index("void AudioEngine::pumpRegionPlayback()") :
        engine.index("void AudioEngine::stopRegionPlaybackMode()")
    ]
    compact = "".join(pump_fn.split())
    assert "ShouldRestartExhaustedUnselectedLoop(" in compact
    # OGG/push-mode keeps totalFrames==0 until the waveform worker resolves it,
    # so the exhaustion restart must be decided before the zero-length bail.
    assert compact.index("ShouldRestartExhaustedUnselectedLoop(") < compact.index(
        "if(m_fileInfo.totalFrames==0)return;"
    )
    assert "m_loopSawPlaying=true" in compact
    # Restart must Seek(0) + Start even when the backend reset the cursor.
    restart_guard = compact[compact.index("ShouldRestartExhaustedUnselectedLoop(") :]
    restart_guard = restart_guard[: restart_guard.index("if(m_fileInfo.totalFrames==0)return;")]
    assert "SeekToFrame(m_sound->Borrow(),0)" in restart_guard
    assert "Start(m_sound->Borrow())" in restart_guard

    assert "inline bool ShouldRestartExhaustedUnselectedLoop" in engine_h
    assert "startGuardFrames" not in engine_h

    play_body = engine[
        engine.index("void AudioEngine::play()") : engine.index("void AudioEngine::pause()")
    ]
    assert "m_loopSawPlaying = m_regionLoopActive" in play_body
    assert play_body.count("m_playIntent = true") == 1
    play_region_body = engine[
        engine.index("void AudioEngine::playRegion") :
        engine.index("void AudioEngine::pumpRegionPlayback")
    ]
    assert "m_loopSawPlaying = m_regionLoopActive" in play_region_body
    reset_fn = engine[
        engine.index("void AudioEngine::resetState") :
        engine.index("std::shared_ptr<const WaveformData> AudioEngine::getWaveformData")
    ]
    assert "m_loopSawPlaying = false" in reset_fn
    load_async = engine[
        engine.index("void AudioEngine::loadFileAsync") :
        engine.index("bool AudioEngine::pumpLoadFileIfPending")
    ]
    assert "m_loopSawPlaying = false" in load_async


def test_unselected_loop_arms_even_when_length_unknown() -> None:
    engine = source(ROOT / "src/AudioEngine.cpp")

    play_region = engine[
        engine.index("void AudioEngine::playRegion") :
        engine.index("void AudioEngine::pumpRegionPlayback")
    ]
    compact_pr = "".join(play_region.split())
    # Audit blocker: playRegion() used to no-op entirely at totalFrames==0,
    # so an OGG first pass was never armed and never started; that is why the
    # user had to click play a second time.
    assert "ShouldArmRegionLoopWithoutLength(" in compact_pr
    assert compact_pr.index("ShouldArmRegionLoopWithoutLength(") < compact_pr.index(
        "if(m_sound&&m_isLoaded&&m_fileInfo.totalFrames>0)"
    )
    unknown_len = compact_pr[
        compact_pr.index("ShouldArmRegionLoopWithoutLength(") :
        compact_pr.index("if(m_sound&&m_isLoaded&&m_fileInfo.totalFrames>0)")
    ]
    assert "m_regionPlayback=true" in unknown_len
    assert "m_regionLoopActive=true" in unknown_len
    assert "m_playIntent=true" in unknown_len
    assert "Start(m_sound->Borrow())" in unknown_len
    # No stop_time/fade bookkeeping that would require a known length.
    assert "SetStopTimeInPcmFrames" in unknown_len
    assert "SeekToFrame" not in unknown_len

    play_body = engine[
        engine.index("void AudioEngine::play()") : engine.index("void AudioEngine::pause()")
    ]
    compact_play = "".join(play_body.split())
    assert "ShouldArmRegionLoopWithoutLength(" in compact_play
    arm_gate = compact_play[
        compact_play.index("ShouldArmRegionLoopWithoutLength(") - 80 :
        compact_play.index("ShouldArmRegionLoopWithoutLength(") + 120
    ]
    assert "m_userLoopEnabled" in arm_gate
    assert "m_regionPlayback" in arm_gate
    # Every explicit Start must mirror the armed-loop state into the saw-playing
    # latch, so the pump can tell an exhausted first pass from a
    # not-yet-started one on the very next tick.
    play_region_all = engine[
        engine.index("void AudioEngine::playRegion") :
        engine.index("void AudioEngine::pumpRegionPlayback")
    ]
    assert "m_loopSawPlaying = false" not in play_region_all
    assert play_region_all.count("m_loopSawPlaying = m_regionLoopActive") == 2


def test_loop_off_cancels_running_loop_in_the_same_tick() -> None:
    workflow = source(ROOT / "src/frontend/AuditionNativeIntentWorkflow.cpp")
    adapter_h = source(ADAPTER_H)
    adapter = source(ADAPTER_CPP)
    engine_h = source(ROOT / "src/AudioEngine.h")
    engine = source(ROOT / "src/AudioEngine.cpp")

    loop_fn = workflow[
        workflow.index("void AuditionNativeIntentWorkflow::setLoopAndRearmRegion") :
    ]
    flat = " ".join(loop_fn.split())
    assert "effects.setRegionLoopEnabled(enabled)" in flat
    assert "if (!enabled)" in flat
    # Unselected full-sound pass: disarm in place instead of restarting.
    assert "effects.stopLoopKeepCurrentPass()" in flat
    assert flat.index("effects.stopLoopKeepCurrentPass()") < flat.index("playRegion(true")
    # Boxed pass re-arms the same region without looping (playRegion(false)).
    assert "playRegion( false, effects.regionFadeIn(), effects.regionFadeOut())" in flat

    assert "void stopLoopKeepCurrentPass() override;" in adapter_h
    assert "state_.audioEngine.stopLoopKeepCurrentPass();" in adapter

    assert "void stopLoopKeepCurrentPass();" in engine_h
    assert "inline LoopDisarmState DecideLoopDisarmKeepCurrentPass" in engine_h
    stop_fn = engine[
        engine.index("void AudioEngine::stopLoopKeepCurrentPass") :
        engine.index("std::string AudioEngine::getLastLoadError()")
    ]
    flat_stop = " ".join(stop_fn.split())
    assert "m_userLoopEnabled = false" in flat_stop
    assert "DecideLoopDisarmKeepCurrentPass(boxedPlayback)" in flat_stop
    assert "m_regionLoopActive = disarm.regionLoopActive" in flat_stop
    assert "m_playIntent = false" in flat_stop
    # Cancelling the loop must not pause/stop/seek or (re)start playback.
    for banned in ("Platform::AudioPlayback::Stop", "SeekToFrame", "Start("):
        assert banned not in flat_stop
    # A disarmed loop (playIntent cleared) must not wrap.
    assert "playing,m_playIntent" in "".join(
        engine[engine.index("void AudioEngine::pumpRegionPlayback") :
               engine.index("void AudioEngine::stopRegionPlaybackMode")].split()
    )


def test_ui_dispatcher_enforces_owner_thread_before_port_dispatch() -> None:
    header = source(CONTROLLER_H)
    implementation = source(CONTROLLER_CPP)
    assert "class AuditionUiDispatcher" in header
    assert "std::thread::id" in header
    assert "std::this_thread::get_id()" in implementation
    wrong_thread = implementation.find("AuditionDispatchStatus::WrongThread")
    controller_call = implementation.find("controller_.dispatch")
    assert -1 not in (wrong_thread, controller_call)
    assert wrong_thread < controller_call


def test_only_appstate_adapter_crosses_the_application_boundary() -> None:
    assert "AppState.h" not in include_targets(source(ADAPTER_H))
    assert "struct AppState;" in source(ADAPTER_H)
    assert "AppState.h" in include_targets(source(ADAPTER_CPP))
    for path in (CONTRACT, CONTROLLER_H, CONTROLLER_CPP, VIEW_H, VIEW_CPP):
        text = source(path)
        assert "AppState.h" not in include_targets(text), path
        assert "AudioEngine.h" not in include_targets(text), path
        assert "state.audioEngine" not in text, path


def test_normative_control_inventory_is_guarded_at_runtime_markers() -> None:
    main = source(MAIN)
    converted = (
        "visible-footer-playback-primary",
        "visible-footer-playback-secondary",
        "volume",
        "reverse-preserving-region",
        "loop-rearm-region",
        "pitch-frequency-mode",
    )
    native_exceptions = (
        "timecode-seek",
        "keyboard-transport",
        "history-file-load",
        "shuffle-persistence",
        "mono",
        "pan-center-sense",
        "channel-solo",
        "waveform-gesture-data",
        "waveform-derived-meter",
    )
    for marker in converted:
        assert main.count(f"AUDITION_BOUNDARY_INTENT: {marker}") == 1
    for marker in native_exceptions:
        assert main.count(f"AUDITION_NATIVE_EXCEPTION: {marker}") == 1


def test_converted_interaction_blocks_have_no_direct_application_mutation() -> None:
    main = source(MAIN)
    blocks = (
        (
            "AUDITION_BOUNDARY_INTENT: visible-footer-playback-primary",
            'if (hov) HarkenSearch::SetTooltip("%s", isPlaying',
        ),
        (
            "AUDITION_BOUNDARY_INTENT: visible-footer-playback-secondary",
            'if (hov) HarkenSearch::SetTooltip("%s", footerPlaying',
        ),
        ("AUDITION_BOUNDARY_INTENT: volume", "// Mono/Stereo toggle"),
        (
            "AUDITION_BOUNDARY_INTENT: reverse-preserving-region",
            "\n                if (revHov) {",
        ),
        ("AUDITION_BOUNDARY_INTENT: loop-rearm-region", "ThemeInspector::RegisterLastItem"),
        ("AUDITION_BOUNDARY_INTENT: pitch-frequency-mode", "pwWasActive = pwActive;"),
    )
    for start, end in blocks:
        start_index = main.index(start)
        end_index = main.index(end, start_index)
        block = main[start_index:end_index]
        assert "state.audioEngine" not in block, start
        assert "state.uiState.regionLoopEnabled =" not in block, start
        assert "state.settings.pitchMode =" not in block, start
        assert "auditionTransportView." in block, start


def test_production_and_focused_tests_are_registered_once() -> None:
    cmake = source(CMAKE)
    behavior = source(BEHAVIOR_CMAKE)
    for unit in (
        "src/frontend/AuditionFrontendController.cpp",
        "src/frontend/AuditionNativeIntentWorkflow.cpp",
        "src/frontend/AppStateAuditionFrontendAdapter.cpp",
        "src/ui/AuditionTransportView.cpp",
    ):
        assert cmake.count(unit) == 1, unit
    assert behavior.count("add_executable(TestAuditionFrontendController") == 1
    assert behavior.count("tests/test_audition_frontend_controller.cpp") == 1
    assert behavior.count("add_test(NAME TestAuditionFrontendController") == 1
    assert behavior.count("NAME TestAuditionFrontendBoundary") == 1
    assert behavior.count("tests/test_audition_frontend_boundary.py") == 1


def test_loop_without_region_keeps_current_sound_looping() -> None:
    workflow = source(ROOT / "src/frontend/AuditionNativeIntentWorkflow.cpp")
    adapter = source(ADAPTER_CPP)
    engine_h = source(ROOT / "src/AudioEngine.h")
    engine = source(ROOT / "src/AudioEngine.cpp")
    windows_main = source(ROOT / "src/WindowsMain.cpp")

    play_fn = workflow[
        workflow.index("void AuditionNativeIntentWorkflow::toggleVisibleFooterPlayback") :
        workflow.index("void AuditionNativeIntentWorkflow::setPitchControlValue")
    ]
    assert "regionLoopEnabled()" in play_fn
    assert play_fn.index("regionLoopEnabled()") < play_fn.index("effects.play();")
    play_region_guard = play_fn[
        play_fn.index("if (") : play_fn.index("effects.play();")
    ]
    assert "HasVisualRegion" in play_region_guard
    assert "regionLoopEnabled()" in play_region_guard

    loop_fn = workflow[
        workflow.index("void AuditionNativeIntentWorkflow::setLoopAndRearmRegion") :
    ]
    assert "playRegion(true" in loop_fn
    assert "HasVisualRegion" in loop_fn
    assert loop_fn.index("HasVisualRegion") < loop_fn.index("playRegion(true")
    assert "setRegionIn(0.0f)" in loop_fn
    assert "setRegionOut(1.0f)" in loop_fn
    assert loop_fn.index("setRegionIn(0.0f)") < loop_fn.index("playRegion(true")
    assert loop_fn.index("setRegionOut(1.0f)") < loop_fn.index("playRegion(true")
    load_async = engine[
        engine.index("void AudioEngine::loadFileAsync") :
        engine.index("bool AudioEngine::pumpLoadFileIfPending")
    ]
    assert "m_inPoint = 0.0f" in load_async
    assert "m_outPoint = 1.0f" in load_async

    assert "setUserLoopEnabled" in engine_h
    assert "state_.audioEngine.setUserLoopEnabled(enabled);" in adapter
    assert "m_userLoopEnabled" in engine
    load_async = engine[
        engine.index("void AudioEngine::loadFileAsync") :
        engine.index("bool AudioEngine::pumpLoadFileIfPending")
    ]
    assert "m_userLoopEnabled = false" not in load_async
    play_if_ready = engine[
        engine.index("void AudioEngine::playIfLoadReady()") :
        engine.index("std::string AudioEngine::getLastLoadError()")
    ]
    assert "play();" in play_if_ready
    assert "m_userLoopEnabled" in play_if_ready
    assert "playRegion(true" in play_if_ready
    assert "m_inPoint = 0.0f" in play_if_ready
    assert "m_outPoint = 1.0f" in play_if_ready
    pump_fn = engine[
        engine.index("void AudioEngine::pumpRegionPlayback()") :
        engine.index("void AudioEngine::stopRegionPlaybackMode()")
    ]
    assert "ResolveLoopedFullSoundOutFrame" in pump_fn
    assert "DecideUnselectedFullSoundLoopWrap" in pump_fn
    assert "m_userLoopEnabled" in pump_fn
    assert "m_decodedPlaybackTotalFrames" in pump_fn
    assert "ResolveLoopedFullSoundOutFrame(m_fileInfo.totalFrames, outFrame)" not in pump_fn
    install_fn = engine[
        engine.index("bool AudioEngine::installDecodedPcmSourceLocked") :
        engine.index("void AudioEngine::loadFileAsync")
    ]
    assert "m_decodedPlaybackTotalFrames" in install_fn
    reset_fn = engine[
        engine.index("void AudioEngine::resetState") :
        engine.index("std::shared_ptr<const WaveformData> AudioEngine::getWaveformData")
    ]
    assert "m_decodedPlaybackTotalFrames = 0" in reset_fn
    load_async = engine[
        engine.index("void AudioEngine::loadFileAsync") :
        engine.index("bool AudioEngine::pumpLoadFileIfPending")
    ]
    assert "m_decodedPlaybackTotalFrames = 0" in load_async
    play_fn_engine = engine[
        engine.index("void AudioEngine::play()") :
        engine.index("void AudioEngine::pause()")
    ]
    assert "m_userLoopEnabled" in play_fn_engine
    clear_fn = engine[
        engine.index("void AudioEngine::clearInOutPoints()") :
        engine.index("static void ApplyExportSoftClip")
    ]
    assert "m_userLoopEnabled" in clear_fn
    assert "playIfLoadReady();" in windows_main

    load_file = engine[
        engine.index("bool AudioEngine::loadFile(") :
        engine.index("bool AudioEngine::installDecodedPcmSourceLocked")
    ]
    assert "QueryLength" in load_file
    assert "m_fileInfo.totalFrames = length" in load_file
    assert "m_decodedPlaybackTotalFrames = length" not in load_file
    assert "ResolveDecodedPlaybackTotalFramesFromSyncLoad" in load_file
    apply_fn = engine[
        engine.index("bool AudioEngine::applyLoadFileResult(") :
        engine.index("bool AudioEngine::applyDecodedPcmLoadFileResult")
    ]
    assert "m_decodedPlaybackTotalFrames = length" not in apply_fn
    assert "m_decodedPlaybackTotalFrames = fileInfo.totalFrames" not in apply_fn
    compact_ready = "".join(play_if_ready.split())
    assert "if(!m_loadAsyncPlayWhenReady)return;" in compact_ready
    assert compact_ready.index("if(!m_loadAsyncPlayWhenReady)return;") < compact_ready.index(
        "playRegion(true"
    )
    assert compact_ready.index("if(!m_loadAsyncPlayWhenReady)return;") < compact_ready.index(
        "play();"
    )
    restart_block = pump_fn[pump_fn.index("if (m_regionLoopActive)") :]
    assert "playing || unselectedFullSoundLoop" not in restart_block
    assert "ShouldSeekLoopedRegionWhilePlaying" in restart_block
    # Exhausted-source fallback: the pump must be allowed to restart a stopped
    # unselected loop only while the user play intent is armed, and must learn
    # the observed real playable length for the next pass.
    assert "m_playIntent" in restart_block
    assert ",playing,m_playIntent,m_loopSawPlaying)" in "".join(restart_block.split())
    assert "m_decodedPlaybackTotalFrames = cursor" in pump_fn
    play_intent_bodies = {
        "play": engine[engine.index("void AudioEngine::play()") : engine.index("void AudioEngine::pause()")],
        "pause": engine[engine.index("void AudioEngine::pause()") : engine.index("void AudioEngine::stop()")],
        "stop": engine[engine.index("void AudioEngine::stop()") : engine.index("bool AudioEngine::loadReversedAudio")],
        "play_region": engine[engine.index("void AudioEngine::playRegion") : engine.index("void AudioEngine::pumpRegionPlayback")],
        "load_async": load_async,
        "reset": reset_fn,
    }
    assert "m_playIntent = true" in play_intent_bodies["play"]
    assert "m_playIntent = true" in play_intent_bodies["play_region"]
    assert "m_playIntent = false" in play_intent_bodies["pause"]
    assert "m_playIntent = false" in play_intent_bodies["stop"]
    assert "m_playIntent = false" in play_intent_bodies["load_async"]
    assert "m_playIntent = false" in play_intent_bodies["reset"]
    for body in (play_intent_bodies["play"], play_intent_bodies["play_region"]):
        assert "Platform::AudioPlayback::Start" in body
        assert body.index("m_playIntent = true") < body.index("Platform::AudioPlayback::Start")
    helper = engine_h[
        engine_h.index("inline uint64_t ResolveLoopedFullSoundOutFrame") :
        engine_h.index("struct FullSoundLoopWrap")
    ]
    assert "if (decodedLengthFrames == 0) return reportedTotalFrames;" in helper
    assert "if (decodedLengthFrames == 0) return 0;" not in helper
    assert "DecideUnselectedFullSoundLoopWrap" in engine_h
    assert "DecideUnselectedFullSoundLoopWrap" in pump_fn
    assert "m_decodedPlaybackTotalFrames" in pump_fn
