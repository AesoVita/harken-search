#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef ARIS_TEST_SOURCE_ROOT
#define ARIS_TEST_SOURCE_ROOT ""
#endif

int main() {
    std::ifstream in;
    if (ARIS_TEST_SOURCE_ROOT[0] != '\0') {
        in.open(std::filesystem::path(ARIS_TEST_SOURCE_ROOT) / "src/NativeFloatReader.cpp",
                std::ios::binary);
    }
    if (!in) in.open("src/NativeFloatReader.cpp", std::ios::binary);
    if (!in) in.open("../src/NativeFloatReader.cpp", std::ios::binary);
    const std::string source((std::istreambuf_iterator<char>(in)), {});
    int failures = 0;
    auto check = [&](bool ok, const char* label) {
        if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
    };
    check(!source.empty(), "NativeFloatReader source is readable");
    check(source.find("#include \"platform/audio_data_source.h\"") != std::string::npos,
          "decoder cleanup uses data-source PAL");
    check(source.find("Platform::AudioDataSource::InitializeDecoderFromFile(decoder_, path)") != std::string::npos,
          "Windows open path delegates initialization to data-source PAL");
    check(source.find("Utf8ToWidePermissive") == std::string::npos &&
              source.find("ma_decoder_init_file_w") == std::string::npos &&
              source.find("ToWinLongPathIfNeeded") == std::string::npos,
          "Windows path conversion and long-path retry stay behind the PAL");
    check(source.find("metadata_.channels > 0 && metadata_.sampleRate > 0") != std::string::npos,
          "metadata validity remains an explicit open condition");
    check(source.find("metadata_.channels > 0 && metadata_.sampleRate > 0") != std::string::npos &&
              source.find("Platform::AudioDataSource::UninitializeDecoder(") != std::string::npos &&
              source.find("Platform::AudioDataSource::UninitializeDecoder(decoder_)") != std::string::npos &&
              source.find("ma_decoder_uninit(&decoder_)") == std::string::npos,
          "metadata init failure has a decoder cleanup seam");
    return failures == 0 ? 0 : 1;
}
