import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AUDIO_ENGINE = ROOT / "src" / "AudioEngine.cpp"
BEHAVIOR = ROOT / "docs" / "behavior" / "modules" / "audioengine.md"
MAIN = ROOT / "src" / "main.cpp"


def source_region(source: str, start: str, end: str) -> str:
    match = re.search(re.escape(start) + r".*?(?=" + re.escape(end) + r")", source, re.DOTALL)
    if not match:
        raise AssertionError(f"Missing source boundary: {start}")
    return match.group(0)


class MagicEqScopeContractTests(unittest.TestCase):
    def test_magic_eq_does_not_change_independent_export_detection(self):
        source = AUDIO_ENGINE.read_text(encoding="utf-8")
        adjustment_summary = source_region(source, "bool AudioEngine::hasAudioAdjustments()", "bool AudioEngine::exportFileProcessed(")
        self.assertNotIn("MagicEQ", adjustment_summary)

    def test_only_existing_daw_transfer_path_selects_magic_eq_rendering(self):
        main = MAIN.read_text(encoding="utf-8")
        processing = source_region(
            main,
            "static bool TransferHasProcessing(const AudioEngine::TransferProcessingSnapshot& snapshot)",
            "static bool TransferHasRegion(",
        )
        self.assertIn("snapshot.magicEQ.enabled", processing)

    def test_behavior_contract_limits_magic_eq_to_current_file_audition(self):
        source = BEHAVIOR.read_text(encoding="utf-8")
        section = source[source.index("## Magic EQ"): ]
        self.assertIn("\u5f53\u524d\u6587\u4ef6\u5b9e\u65f6\u8bd5\u542c", section)
        self.assertIn("\u72ec\u7acb\u5bfc\u51fa\u83dc\u5355\u7684\u73b0\u6709\u884c\u4e3a", section)
        self.assertIn("DAW", section)


if __name__ == "__main__":
    unittest.main()
