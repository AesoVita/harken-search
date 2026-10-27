import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AUDIO_ENGINE = ROOT / "src" / "AudioEngine.cpp"


class MagicEqMonoPlaybackContractTests(unittest.TestCase):
    def test_mono_decoder_keeps_the_processing_source_in_both_load_paths(self):
        source = AUDIO_ENGINE.read_text(encoding="utf-8")
        self.assertIn("decoderFormat.channels == 1", source)
        self.assertGreaterEqual(source.count("m_centerSenseDataSource->init("), 3)
        self.assertNotIn("TimeStretch", source)


if __name__ == "__main__":
    unittest.main()
