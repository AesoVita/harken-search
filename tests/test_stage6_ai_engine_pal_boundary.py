import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class Stage6AIEnginePalBoundaryContract(unittest.TestCase):
    def test_ai_engine_load_audio_uses_mono_float_reader_pal(self) -> None:
        header = (ROOT / "src" / "platform" / "audio_file.h").read_text(encoding="utf-8")
        source = (ROOT / "src" / "AIEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AIEngine::LoadAudioFile", 1)[1].split(
            "bool AIEngine::SaveAudioFile", 1
        )[0]

        self.assertIn("Hz16000 = 16000", header)
        self.assertIn("Platform::AudioFile::MonoFloatReader", body)
        self.assertIn("Platform::AudioFile::MonoSampleRate::Hz16000", body)
        self.assertIn("Platform::AudioFile::Status::EndOfFile", body)
        for native_token in (
            "ma_decoder",
            "ma_decoder_config_init",
            "ma_decoder_init_file_w",
            "ma_decoder_init_file",
        ):
            self.assertNotIn(native_token, body)

    def test_ai_engine_wininet_operations_use_system_pal(self) -> None:
        header = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
        win32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
        macos = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
        source = (ROOT / "src" / "AIEngine.cpp").read_text(encoding="utf-8")

        for declaration, call in (
            ("void* OpenInternetSession(", "Platform::OpenInternetSession("),
            ("bool SetInternetOption(", "Platform::SetInternetOption("),
            ("void* ConnectInternetHttp(", "Platform::ConnectInternetHttp("),
            ("void* OpenHttpRequest(", "Platform::OpenHttpRequest("),
            ("bool SendHttpRequest(", "Platform::SendHttpRequest("),
            ("bool QueryHttpInfoNumber(", "Platform::QueryHttpInfoNumber("),
            ("void* OpenInternetUrl(", "Platform::OpenInternetUrl("),
            ("bool ReadInternetFile(", "Platform::ReadInternetFile("),
        ):
            self.assertIn(declaration, header)
            self.assertIn(declaration, win32)
            self.assertIn(declaration, macos)
            self.assertIn(call, source)

        for native_token in (
            "InternetOpenW(",
            "InternetSetOptionW(",
            "InternetConnectW(",
            "HttpOpenRequestW(",
            "HttpSendRequestW(",
            "HttpQueryInfoW(",
            "InternetOpenUrlW(",
            "InternetReadFile(",
        ):
            self.assertNotIn(native_token, source)

    def test_ai_engine_model_download_file_write_uses_system_pal(self) -> None:
        header = (ROOT / "src" / "platform" / "system.h").read_text(encoding="utf-8")
        win32 = (ROOT / "src" / "platform" / "system_win32.cpp").read_text(encoding="utf-8")
        macos = (ROOT / "src" / "platform" / "system_macos.mm").read_text(encoding="utf-8")
        source = (ROOT / "src" / "AIEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AIEngine::DownloadModel", 1)[1].split(
            "// 音频加载", 1
        )[0]

        for declaration, call in (
            ("void* OpenExclusiveWriteFile(", "Platform::OpenExclusiveWriteFile("),
            ("bool WriteFileBytes(", "Platform::WriteFileBytes("),
            ("void CloseFileHandle(", "Platform::CloseFileHandle("),
        ):
            self.assertIn(declaration, header)
            self.assertIn(declaration, win32)
            self.assertIn(declaration, macos)
            self.assertIn(call, body)

        self.assertIn("GENERIC_WRITE", win32)
        self.assertIn("CREATE_ALWAYS", win32)
        self.assertIn("WriteFile(", win32)
        self.assertIn("CloseHandle(", win32)
        self.assertNotIn("CreateFileW", body)
        self.assertNotIn("WriteFile(hFile", body)
        self.assertNotIn("CloseHandle(", body)

    def test_ai_engine_save_audio_uses_audio_file_writer_pal(self) -> None:
        header = (ROOT / "src/platform/audio_file.h").read_text(encoding="utf-8")
        source = (ROOT / "src/AIEngine.cpp").read_text(encoding="utf-8")
        body = source.split("bool AIEngine::SaveAudioFile", 1)[1].split(
            "// 语音识别", 1
        )[0]

        self.assertIn("class FloatWavWriter final", header)
        self.assertIn("Platform::AudioFile::FloatWavWriter writer;", body)
        self.assertIn("writer.Open(path, sampleRate, channels)", body)
        self.assertIn("writer.Write(", body)
        self.assertIn("pcmData.data()", body)
        self.assertIn("writer.Close()", body)
        self.assertNotIn("miniaudio.h", source)
        for native_token in (
            "ma_encoder_config",
            "ma_encoder_init_file_w",
            "ma_encoder_init_file(",
            "ma_encoder_write_pcm_frames",
            "ma_encoder_uninit",
        ):
            self.assertNotIn(native_token, body)


if __name__ == "__main__":
    unittest.main()
