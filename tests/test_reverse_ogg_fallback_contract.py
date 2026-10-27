"""BUG-20260916-01 反向加载必须支持 push 模式 OGG（length 恒为 0）。

OGG/Vorbis 经 miniaudio 的 stb_vorbis push 解码：init 成功、可顺序读 PCM，
但 ma_data_source_get_length_in_pcm_frames 恒为 0（见 AudioEngine.cpp 内注释）。
旧 loadReversedAudio 先 QueryLength，再 `if (totalFrames == 0) return false;`，
并用一次性 ReadPcmFrames(totalFrames) —— 对 OGG 直接放弃，导致点反转无声音（假成功）。

本契约锁定根因修复：反向必须与 DecodeFileToInterleavedF32 一样顺序读到耗尽，
不依赖总长度。WAV/FLAC/MP3 路径仍走同一顺序读，行为不变。
"""

from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _reverse_body() -> str:
    source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
    return source.split("bool AudioEngine::loadReversedAudio()", 1)[1].split(
        "bool AudioEngine::reloadWithReverseState", 1
    )[0]


class ReverseOggFallbackContract(unittest.TestCase):
    def test_no_zero_length_bail(self) -> None:
        body = _reverse_body()
        # 旧缺陷：把 length==0 当失败直接 return，OGG push 模式恒为 0 即被误杀。
        self.assertNotIn("if (totalFrames == 0)", body)

    def test_reads_sequentially_until_exhausted(self) -> None:
        body = _reverse_body()
        self.assertIn("while (", body)
        self.assertIn("Platform::AudioDataSource::ReadPcmFrames(", body)
        self.assertIn("m_reverseBuffer.insert(", body)
        # Greptile P1：损坏 OGG 中途解码错误不得当 EOF。干净结束看 backendCode，
        # 不能只靠 framesRead==0 break。
        self.assertIn("readResult.backendCode", body)
        self.assertIn("MA_AT_END", body)
        self.assertNotRegex(body, r"if \(framesRead == 0\) break;")

    def test_keeps_oom_guard(self) -> None:
        # 仍必须有 300s 上限防 OOM（既有 duration 守卫 + 解码后字节数兜底）。
        body = _reverse_body()
        self.assertIn("300", body)

    def test_preserves_existing_channel_swap_contract(self) -> None:
        # 不破坏 test_audio_engine_waveform_internal_types.py 既有契约。
        body = _reverse_body()
        self.assertIn("std::uint32_t channels = m_forceMono ? 1 : 2;", body)
        self.assertIn("for (uint32_t ch = 0; ch < channels; ch++)", body)
        self.assertNotIn("for (ma_uint32 ch = 0; ch < channels; ch++)", body)


class ReverseNoFakeSuccessContract(unittest.TestCase):
    """a3/a4：真做不成时不进入开启态并给中文原因；两入口一致。"""

    def test_engine_reconciles_reverse_without_sound(self) -> None:
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::setReverse(bool reverse)", 1)[1].split(
            "bool AudioEngine::loadReversedAudio", 1
        )[0]
        self.assertIn("if (m_reverse && !m_sound) m_reverse = false;", body)


    def test_reverse_load_failure_restores_forward_sound(self) -> None:
        import re
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::reloadWithReverseState()", 1)[1].split(
            "void AudioEngine::setInPoint", 1
        )[0]
        fail = body.split("if (!loaded || m_reverseBuffer.empty())", 1)[1]
        before_pcm = fail.split("InitializePcmBuffer", 1)[0]
        self.assertIn("restoreForwardAfterReverseFailure = true", before_pcm)
        self.assertIsNone(
            re.search(r"m_reverse = false;\s*return false;", before_pcm),
            "reverse decode failure must restore forward sound, not return silent",
        )
        self.assertIn("if (restoreForwardAfterReverseFailure)", body)
        self.assertIn("installForwardSound", body)
        self.assertIn("return !restoreForwardAfterReverseFailure;", body)
        if "bool AudioEngine::setReverse(bool reverse)" in source:
            set_reverse = source.split("bool AudioEngine::setReverse(bool reverse)", 1)[1].split(
                "bool AudioEngine::loadReversedAudio", 1
            )[0]
        else:
            set_reverse = source.split("void AudioEngine::setReverse(bool reverse)", 1)[1].split(
                "bool AudioEngine::loadReversedAudio", 1
            )[0]
        self.assertIn("originalPos", set_reverse)
        self.assertIn("reloaded ? pos : originalPos", set_reverse)

    def test_unloaded_set_reverse_clears_flag_before_early_return(self) -> None:
        source = (ROOT / "src" / "AudioEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AudioEngine::setReverse(bool reverse)", 1)[1].split(
            "bool AudioEngine::loadReversedAudio", 1
        )[0]
        unloaded, _, rest = body.partition("if (!m_isLoaded || m_fileInfo.filePath.empty())")
        self.assertTrue(unloaded)
        early, _, after = rest.partition("needReload = true;")
        self.assertIn("if (reverse) m_reverse = false;", early)
        self.assertNotIn("if (!m_isLoaded || m_fileInfo.filePath.empty()) return;", body)
        self.assertIn("if (m_reverse && !m_sound) m_reverse = false;", after)

    def test_load_reversed_treats_decode_error_as_failure_not_eof(self) -> None:
        body = _reverse_body()
        self.assertIn("readResult.backendCode", body)
        self.assertIn("MA_AT_END", body)
        self.assertIn("decodeErrored", body)
        self.assertIn("Reverse aborted: decode error before EOF", body)
        self.assertRegex(body, r"if \(decodeErrored\)")
        self.assertIn("return false;", body.split("if (decodeErrored)", 1)[1].split("if (decodedFrames == 0)", 1)[0])

    def test_both_native_entries_reference_failure_key(self) -> None:
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        # 快捷键入口 + 底栏按钮入口都要在开启失败时给中文原因。
        self.assertGreaterEqual(main.count("L(Player_ReverseFailed)"), 2)

        reverse_block = main.split(
            "AUDITION_BOUNDARY_INTENT: reverse-preserving-region", 1
        )[1].split(
            chr(10) + "                if (revHov) {", 1
        )[0]
        self.assertNotIn("state.audioEngine", reverse_block)
        self.assertIn("auditionDispatcher.captureSnapshot(reverseAfter)", reverse_block)
        self.assertIn("!reverseAfter.reversed", reverse_block)

    def test_failure_key_is_trilingual_pure(self) -> None:
        import json
        import re
        entries = json.loads((ROOT / "locale" / "harken.ui.strings.json").read_text(encoding="utf-8"))["entries"]
        ent = next(e for e in entries if e["id"] == "Player_ReverseFailed")
        cjk = re.compile(r"[一-鿿]")
        kana = re.compile(r"[぀-ヿ]")
        self.assertTrue(cjk.search(ent["zh"]))
        self.assertFalse(cjk.search(ent["ja"]) and not kana.search(ent["ja"]))
        self.assertTrue(kana.search(ent["ja"]))  # ja 列必须是日语，不能填中文
        self.assertTrue(ent["en"].isascii())
        self.assertNotEqual(ent["zh"], ent["ja"])


if __name__ == "__main__":
    unittest.main()
