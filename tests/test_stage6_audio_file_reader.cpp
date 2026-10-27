#include "platform/audio_file.h"
#include "audio_file_miniaudio_test_seam.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
using namespace Platform::AudioFile;
int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}
bool Matches(ReadResult result, Status status, FrameCount frames) { return result.status == status && result.framesRead == frames; }

std::filesystem::path WriteFixture(bool unicode = false) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto name = unicode ? std::filesystem::u8path("aris_音效_" + std::to_string(stamp) + ".wav")
                              : std::filesystem::path("aris_audio_reader_" + std::to_string(stamp) + ".wav");
    const auto path = std::filesystem::temp_directory_path() / name;
    const std::array<unsigned char, 52> wav = {
        'R','I','F','F',0x2C,0,0,0,'W','A','V','E','f','m','t',' ',
        16,0,0,0,1,0,1,0,0x22,0x56,0,0,0x44,0xAC,0,0,2,0,16,0,
        'd','a','t','a',8,0,0,0,0,0,1,0,2,0,3,0};
    std::ofstream(path, std::ios::binary).write(
        reinterpret_cast<const char*>(wav.data()), wav.size());
    return path;
}

std::filesystem::path WriteStereoFixture() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("aris_audio_reader_stereo_" + std::to_string(stamp) + ".wav");
    const std::array<unsigned char, 44> header = {
        'R','I','F','F',0x24,1,0,0,'W','A','V','E','f','m','t',' ',
        16,0,0,0,1,0,2,0,0x44,0xAC,0,0,0x10,0xB1,2,0,4,0,16,0,
        'd','a','t','a',0,1,0,0};
    const std::array<unsigned char, 4> frame = {0xE8,0x03,0xB8,0x0B};
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(header.data()), header.size());
    for (int i = 0; i < 64; ++i)
        out.write(reinterpret_cast<const char*>(frame.data()), frame.size());
    return path;
}

void RunBasic() {
    const std::array<float, 7> conversionInput = {
        -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f};
    std::array<std::uint8_t, 21> packedS24{};
    ConvertFloatToPackedS24(
        packedS24.data(), conversionInput.data(), conversionInput.size());
    const std::array<std::uint8_t, 21> expectedPackedS24 = {
        0x01, 0x00, 0x80,
        0x01, 0x00, 0x80,
        0x01, 0x00, 0xC0,
        0x00, 0x00, 0x00,
        0xFF, 0xFF, 0x3F,
        0xFF, 0xFF, 0x7F,
        0xFF, 0xFF, 0x7F,
    };
    Check(packedS24 == expectedPackedS24,
          "basic: float samples convert to clipped little-endian packed S24");
    std::array<std::uint8_t, 3> zeroCountSentinel = {0xA5, 0xA5, 0xA5};
    ConvertFloatToPackedS24(
        zeroCountSentinel.data(), conversionInput.data(), 0);
    Check(zeroCountSentinel == std::array<std::uint8_t, 3>{0xA5, 0xA5, 0xA5},
          "basic: zero-sample S24 conversion preserves destination");

    LinearFloatResampler resampler;
    Check(!resampler.Open(0, 24000, 48000),
          "basic: resampler rejects zero channels");
    Check(resampler.Open(1, 24000, 48000),
          "basic: linear float resampler opens");
    std::array<float, 16> resampleInput{};
    for (std::size_t index = 0; index < resampleInput.size(); ++index) {
        resampleInput[index] = static_cast<float>(index) / 16.0f;
    }
    std::array<float, 4> resampleOutput{};
    const auto progress = resampler.Process(
        resampleInput.data(), resampleInput.size(),
        resampleOutput.data(), resampleOutput.size());
    Check(progress.backendSucceeded &&
              progress.inputConsumed > 0 &&
              progress.inputConsumed < resampleInput.size() &&
              progress.outputProduced > 0 &&
              progress.outputProduced <= resampleOutput.size(),
          "basic: bounded resampler output partially consumes input");
    const auto drain = resampler.Drain(
        resampleOutput.data(), resampleOutput.size());
    Check(drain.backendSucceeded && drain.inputConsumed == 0 &&
              drain.outputProduced <= resampleOutput.size(),
          "basic: one drain step uses no input and respects output capacity");
    LinearFloatResampler movedResampler(std::move(resampler));
    Check(!resampler.IsOpen() && movedResampler.IsOpen(),
          "basic: resampler move construction closes source");
    movedResampler.Close();
    movedResampler.Close();
    Check(!movedResampler.IsOpen(),
          "basic: repeated resampler close is idempotent");

    const auto path = WriteFixture();
    MonoFloatReader reader;
    Check(reader.Open(path.string(),
                      LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) ==
              Status::Ok,
          "basic: generated WAV opens");
    float buffer[3] = {};
    const auto read = reader.Read(buffer, 3);
    Check(read.status == Status::Ok && read.framesRead == 3,
          "basic: positive read advances");
    Check(reader.Seek(1) == Status::Ok, "basic: absolute seek succeeds");
    const auto postSeek = reader.Read(buffer, 1);
    Check(postSeek.status == Status::Ok && postSeek.framesRead == 1,
          "basic: post-seek read advances");
    Check(reader.Seek(3) == Status::Ok, "basic: seek before short read succeeds");
    const auto shortRead = reader.Read(buffer, 3);
    Check(shortRead.framesRead > 0 && shortRead.framesRead < 3 &&
              (shortRead.status == Status::Ok || shortRead.status == Status::EndOfFile),
          "basic: real backend returns a positive short read");
    Check(Matches(reader.Read(buffer, 1), Status::EndOfFile, 0),
          "basic: read after short result reaches EOF");
    Check(reader.Seek(4) == Status::Ok, "basic: seek to output EOF succeeds");
    const auto eof = reader.Read(buffer, 1);
    Check(eof.status == Status::EndOfFile && eof.framesRead == 0,
          "basic: short read reaches EOF");
    reader.Seek(0);
    std::fill(std::begin(buffer), std::end(buffer), 7.0f);
    std::atomic<bool> cancel{true};
    const auto cancelled = reader.Read(buffer, 1, &cancel);
    Check(cancelled.status == Status::Cancelled && cancelled.framesRead == 0 && buffer[0] == 7.0f,
          "basic: cancellation preserves destination");
    MonoFloatReader moved(std::move(reader));
    Check(!reader.IsOpen() && moved.IsOpen(), "basic: move construction closes source");
    MonoFloatReader assigned;
    assigned = std::move(moved);
    Check(!moved.IsOpen() && assigned.IsOpen(), "basic: move assignment closes source");
    assigned.Close();
    assigned.Close();
    Check(!assigned.IsOpen(), "basic: repeated close is idempotent");
    const auto corrupt = path.parent_path() / (path.stem().string() + "_corrupt.bin");
    std::ofstream(corrupt, std::ios::binary) << "not-a-wave";
    MonoFloatReader bad;
    Check(bad.Open(corrupt.string(), LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) ==
              Status::OpenFailed, "basic: corrupt input fails open");
    const auto stereo = WriteStereoFixture();
    MonoFloatReader converted;
    Check(converted.Open(stereo.string(), LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) ==
              Status::Ok, "basic: stereo 44100 fixture opens");
    std::array<float, 8> mono{}, warmup{};
    converted.Read(warmup.data(), warmup.size());
    const auto convertedRead = converted.Read(mono.data(), mono.size());
    Check(convertedRead.framesRead > 0, "basic: resampler produces mono frames");
    for (FrameCount i = 0; i < convertedRead.framesRead; ++i)
        Check(std::isfinite(mono[static_cast<std::size_t>(i)]) &&
                  std::fabs(mono[static_cast<std::size_t>(i)] - 2000.0f / 32768.0f) < 0.002f,
              "basic: downmixed float matches constant average");
    const auto unicode = WriteFixture(true);
    MonoFloatReader unicodeReader;
    Check(unicodeReader.Open(unicode.u8string(), LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) ==
              Status::Ok && unicodeReader.Read(mono.data(), 1).framesRead == 1,
          "basic: Unicode path opens and reads");
    converted.Close();
    unicodeReader.Close();
    Check(!converted.IsOpen() && !unicodeReader.IsOpen(),
          "basic: fixture readers close before cleanup");
    reader.Close();
    Check(!reader.IsOpen(), "basic: close ends reader lifetime");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(corrupt, ignored);
    std::filesystem::remove(stereo, ignored);
    std::filesystem::remove(unicode, ignored);
}

void RunStatus() {
    using namespace Platform::AudioFile::TestSeam;
    Reset();
    Install();
    std::array<float, 3> samples{};
    MonoFloatReader failedInit;
    QueueInit(MA_ERROR);
    const auto failedStatus = failedInit.Open("__force_init_failure__",
                          LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix,
                          MonoSampleRate::Hz44100);
    Check(failedStatus == Status::OpenFailed,
          "status: init failure maps to OpenFailed");
    Check(!failedInit.IsOpen(), "status: init failure leaves reader closed");
    Check(!Get().outputSampleRates.empty() &&
              std::all_of(Get().outputSampleRates.begin(), Get().outputSampleRates.end(),
                          [](ma_uint32 rate) { return rate == 44100; }),
          "status: requested output sample rate reaches decoder config");
    Check(Matches(failedInit.Read(samples.data(), 1), Status::InvalidArgument, 0),
          "status: read after init failure is unopened");

    MonoFloatReader unopened;
    Check(Matches(unopened.Read(samples.data(), 1), Status::InvalidArgument, 0),
          "status: unopened nonzero read is invalid");
    MonoFloatReader opened;
    QueueInit(MA_SUCCESS);
    Check(opened.Open("__force_seek_failure__",
                      LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) ==
              Status::Ok,
          "status: seam open succeeds");
    Check(Matches(opened.Read(nullptr, 1), Status::InvalidArgument, 0),
          "status: null nonzero read is invalid");
    const auto beforePositive = Get().readCallCount;
    QueueRead(MA_SUCCESS, 2);
    Check(Matches(opened.Read(samples.data(), 2), Status::Ok, 2),
          "status: MA_SUCCESS positive maps Ok");
    Check(Get().readCallCount == beforePositive + 1,
          "status: positive read increments callback count");
    QueueRead(MA_SUCCESS, 0);
    Check(Matches(opened.Read(samples.data(), 2), Status::EndOfFile, 0),
          "status: MA_SUCCESS zero maps EndOfFile");
    QueueRead(MA_AT_END, 3);
    Check(Matches(opened.Read(samples.data(), 3), Status::EndOfFile, 3),
          "status: MA_AT_END retains frames");
    QueueRead(MA_ERROR, 2);
    Check(Matches(opened.Read(samples.data(), 2), Status::DecodeFailed, 2),
          "status: hard result retains frames");
    const std::size_t beforeCancel = Get().readCallCount;
    std::atomic<bool> cancel{true};
    Check(Matches(opened.Read(samples.data(), 1, &cancel), Status::Cancelled, 0),
          "status: cancellation prevents read");
    Check(Get().readCallCount == beforeCancel,
          "status: cancellation avoids callback");
    const std::size_t beforeZero = Get().readCallCount;
    const auto zero = unopened.Read(samples.data(), 0);
    Check(zero.status == Status::Ok && zero.framesRead == 0,
          "status: zero request is Ok,0");
    Check(Get().readCallCount == beforeZero,
          "status: zero request avoids callback");
    QueueSeek(MA_ERROR);
    Check(opened.Seek(42) == Status::SeekFailed,
          "status: backend seek failure maps to SeekFailed");
}

void RunPathFallbacks() {
#ifdef _WIN32
    using namespace Platform::AudioFile::TestSeam;
    auto check = [](const std::string& path, LegacyWindowsPathPolicy policy,
                    const std::wstring& first, const std::wstring& second,
                    std::size_t attempts) {
        Reset(); Install(); QueueInit(MA_ERROR);
        if (attempts == 2) QueueInit(MA_SUCCESS);
        MonoFloatReader reader;
        Check(reader.Open(path, policy) == (attempts == 2 ? Status::Ok : Status::OpenFailed),
              "path: expected open result");
        const auto& candidates = Get().wideCandidates;
        Check(candidates.size() == attempts, "path: expected attempt count");
        if (!candidates.empty()) Check(candidates[0] == first, "path: first candidate");
        if (candidates.size() > 1) Check(candidates[1] == second, "path: fallback candidate");
    };
    check("C:/音效.wav", LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix,
          L"C:/音效.wav", L"\\\\?\\C:/音效.wav", 2);
    check("C:/音效.wav", LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix,
          L"C:/音效.wav", L"\\\\?\\C:\\音效.wav", 2);
    check("//server/share/x.wav", LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix,
          L"//server/share/x.wav", L"\\\\?\\UNC\\server\\share\\x.wav", 2);
    check("\\\\?\\C:\\x.wav", LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix,
          L"\\\\?\\C:\\x.wav", L"", 1);
    check("relative.wav", LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix,
          L"relative.wav", L"", 1);
#endif
}
} // namespace

int main(int argc, char** argv) {
    const std::string group = argc > 2 && std::string(argv[1]) == "--group"
                                  ? argv[2] : "";
    if (group == "basic") { RunBasic(); RunPathFallbacks(); }
    else if (group == "status") RunStatus();
    else if (group == "path") RunPathFallbacks();
    else return 2;
    return failures == 0 ? 0 : 1;
}
