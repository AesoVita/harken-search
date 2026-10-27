#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    std::string source;
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream in(repoRoot / "src/WaveformExtractor.cpp", std::ios::binary);
    if (in) source.assign((std::istreambuf_iterator<char>(in)), {});
    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
    };
    check(!source.empty(), "WaveformExtractor source is readable");
    check(source.find("InitDecoderForPath") == std::string::npos,
          "legacy decoder initializer is retired after NativeFloatReader migration");
    check(source.find("ma_decoder_") == std::string::npos,
          "WaveformExtractor has no direct miniaudio decoder calls");
    check(source.find("ma_uint32") == std::string::npos,
          "WaveformExtractor uses std uint32 ownership");
    check(source.find("ma_uint64") == std::string::npos,
          "WaveformExtractor uses std uint64 ownership");
    check(source.find("thirdparty/miniaudio.h") == std::string::npos,
          "WaveformExtractor no longer includes miniaudio");
    check(source.find("NativeFloatReader") != std::string::npos,
          "WaveformExtractor retains NativeFloatReader ownership");
    return failures == 0 ? 0 : 1;
}
