#include "../src/platform/database_audio_probe.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace Platform::DatabaseAudioProbe {
namespace {

uint16_t Read16(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t Read32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

} // namespace

bool Probe(const std::filesystem::path& path, Info& out)
{
    out = {};
    std::ifstream file(path, std::ios::binary);
    std::array<uint8_t, 44> header{};
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    if (file.gcount() != static_cast<std::streamsize>(header.size()) ||
        std::memcmp(header.data(), "RIFF", 4) != 0 ||
        std::memcmp(header.data() + 8, "WAVEfmt ", 8) != 0 ||
        std::memcmp(header.data() + 36, "data", 4) != 0) {
        return false;
    }

    out.channels = Read16(header.data() + 22);
    out.sampleRate = Read32(header.data() + 24);
    const uint16_t blockAlign = Read16(header.data() + 32);
    const uint32_t dataBytes = Read32(header.data() + 40);
    if (out.channels == 0 || out.sampleRate == 0 || blockAlign == 0) {
        out = {};
        return false;
    }
    out.totalFrames = dataBytes / blockAlign;
    out.durationSeconds = static_cast<float>(out.totalFrames) /
                          static_cast<float>(out.sampleRate);
    return out.totalFrames > 0;
}

} // namespace Platform::DatabaseAudioProbe
