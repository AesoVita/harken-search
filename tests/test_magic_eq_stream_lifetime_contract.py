from pathlib import Path


ROOT = Path(__file__).parents[1]
CPP = (ROOT / "src/AudioEngine.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/AudioEngine.h").read_text(encoding="utf-8")


def test_multichannel_pcm_disables_unavailable_magic_eq():
    install = CPP.split("bool AudioEngine::installDecodedPcmSourceLocked", 1)[1].split(
        "void AudioEngine::unload", 1
    )[0]
    availability = CPP.split("void AudioEngine::enforceMagicEQAvailabilityLocked", 1)[1].split(
        "MagicEQSettings AudioEngine::getMagicEQ", 1
    )[0]
    assert "m_playbackChannels = fileInfo.channels" in install
    assert "m_playbackChannels > 2" in availability
    assert "m_magicEQ.enabled = false" in availability
    assert "m_magicEQ.invalidInput = true" in availability
    reverse = CPP.split("bool AudioEngine::reloadWithReverseState(", 1)[1].split("void AudioEngine::setInPoint", 1)[0]
    assert "m_playbackChannels = format.channels" in reverse
    assert "m_playbackChannels = channels" in reverse


def test_deferred_stream_keeps_decoder_and_processing_source_until_cleanup():
    assert "CenterSenseDataSource* csds" in HEADER
    assert CPP.count("dc.csds = oldCSDS") == 3
    assert CPP.count("dc.decoder = std::move(oldDecoder)") == 3
    assert "TimeStretch" not in HEADER
    assert "TimeStretch" not in CPP
