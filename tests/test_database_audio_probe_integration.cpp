#include "platform/database_audio_probe.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Platform::DatabaseAudioProbe::Info;
using Platform::DatabaseAudioProbe::Probe;

namespace {
int failures = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::printf("FAIL: %s\n", message);
        ++failures;
    }
}

void U16(std::vector<unsigned char>& bytes, uint16_t value) {
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

void U32(std::vector<unsigned char>& bytes, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void U64(std::vector<unsigned char>& bytes, uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        bytes.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void Tag(std::vector<unsigned char>& bytes, const char* tag) {
    bytes.insert(bytes.end(), tag, tag + 4);
}

std::vector<unsigned char> WaveBytes(bool rf64) {
    std::vector<unsigned char> bytes;
    Tag(bytes, rf64 ? "RF64" : "RIFF");
    U32(bytes, rf64 ? 0xffffffffu : 44u);
    Tag(bytes, "WAVE");
    if (rf64) {
        Tag(bytes, "ds64");
        U32(bytes, 28);
        U64(bytes, 80);
        U64(bytes, 8);
        U64(bytes, 4);
        U32(bytes, 0);
    }
    Tag(bytes, "fmt ");
    U32(bytes, 16);
    U16(bytes, 1);
    U16(bytes, 1);
    U32(bytes, 22050);
    U32(bytes, 44100);
    U16(bytes, 2);
    U16(bytes, 16);
    Tag(bytes, "data");
    U32(bytes, rf64 ? 0xffffffffu : 8u);
    bytes.insert(bytes.end(), 8, 0);
    return bytes;
}

bool Write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

bool Same(const Info& actual, const Info& expected) {
    return actual.sampleRate == expected.sampleRate &&
           actual.channels == expected.channels &&
           actual.totalFrames == expected.totalFrames &&
           std::fabs(actual.durationSeconds - expected.durationSeconds) < 0.000001f;
}

fs::path UniqueTempDirectory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int suffix = 0; suffix < 1000; ++suffix) {
        const fs::path candidate = fs::temp_directory_path() /
            ("aris_database_probe_" + std::to_string(stamp) + "_" + std::to_string(suffix));
        std::error_code error;
        if (fs::create_directory(candidate, error)) {
            return candidate;
        }
    }
    return {};
}
} // namespace

int main() {
    const fs::path base = UniqueTempDirectory();
    Check(!base.empty(), "unique temporary directory created");
    if (base.empty()) {
        return 1;
    }

    const auto riffBytes = WaveBytes(false);
    const auto rf64Bytes = WaveBytes(true);
    Check(riffBytes.size() == 52 && riffBytes[4] == 44 &&
          riffBytes[5] == 0 && riffBytes[6] == 0 && riffBytes[7] == 0,
          "PCM fixture has exact RIFF size 44 and 52 bytes");

    const fs::path riff = base / "valid.wav";
    const fs::path rf64 = base / "valid.wave";
    const fs::path malformed = base / "malformed.wav";
    const fs::path unicode = base / fs::u8path(u8"音频.wav");
    const fs::path disguised = base / "content.probe-audio";
    Check(Write(riff, riffBytes), "write RIFF fixture");
    Check(Write(rf64, rf64Bytes), "write RF64 fixture");
    Check(Write(unicode, riffBytes), "write Unicode fixture");
    Check(Write(disguised, riffBytes), "write disguised WAV fixture");
    Check(Write(malformed, {'b', 'a', 'd'}), "write malformed fixture");

    const Info expected{22050, 1, 4, 4.0f / 22050.0f};
    for (const auto& path : {riff, rf64, unicode}) {
        Info out{};
        Check(Probe(path, out), "generated WAV fast path succeeds");
        Check(Same(out, expected), "generated WAV reports exact info");
    }
    Info malformedInfo{9, 9, 9, 9.0f};
    Check(!Probe(malformed, malformedInfo), "malformed WAV fails all probes");
    Info disguisedInfo{};
    Check(Probe(disguised, disguisedInfo), "disguised WAV content is detected");
    Check(Same(disguisedInfo, expected), "disguised WAV reports 22050/1/4");

    std::error_code cleanupError;
    fs::remove_all(base, cleanupError);
    Check(!cleanupError && !fs::exists(base), "temporary fixtures removed");
    return failures == 0 ? 0 : 1;
}
