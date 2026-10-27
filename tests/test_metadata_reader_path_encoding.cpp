#include "MetadataReader.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

struct TempRoot {
    fs::path path = fs::temp_directory_path() /
        ("aris_metadata_path_encoding_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    TempRoot() { fs::create_directories(path); }
    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void AppendAscii(std::vector<uint8_t>& bytes, const char* text, size_t count) {
    bytes.insert(bytes.end(), text, text + count);
}

void AppendLe32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

void AppendBe32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
    }
}

void StoreLe32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes[offset++] = static_cast<uint8_t>((value >> shift) & 0xFF);
    }
}

void AppendSyncSafe32(std::vector<uint8_t>& bytes, uint32_t value) {
    bytes.push_back(static_cast<uint8_t>((value >> 21) & 0x7F));
    bytes.push_back(static_cast<uint8_t>((value >> 14) & 0x7F));
    bytes.push_back(static_cast<uint8_t>((value >> 7) & 0x7F));
    bytes.push_back(static_cast<uint8_t>(value & 0x7F));
}

bool WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool WriteId3v1(const fs::path& path, const std::string& title) {
    std::vector<uint8_t> bytes(128, 0);
    bytes[0] = 'T';
    bytes[1] = 'A';
    bytes[2] = 'G';
    const size_t count = std::min<size_t>(30, title.size());
    std::copy_n(title.begin(), count, bytes.begin() + 3);
    return WriteBytes(path, bytes);
}

bool WriteBextWav(const fs::path& path, const std::string& description) {
    std::vector<uint8_t> bytes;
    AppendAscii(bytes, "RIFF", 4);
    AppendLe32(bytes, 0);
    AppendAscii(bytes, "WAVE", 4);
    AppendAscii(bytes, "bext", 4);
    AppendLe32(bytes, 602);
    const size_t dataStart = bytes.size();
    bytes.resize(dataStart + 602, 0);
    const size_t count = std::min<size_t>(255, description.size());
    std::copy_n(description.begin(), count, bytes.begin() + dataStart);
    StoreLe32(bytes, 4, static_cast<uint32_t>(bytes.size() - 8));
    return WriteBytes(path, bytes);
}

bool WriteId3Apic(const fs::path& path, const std::vector<uint8_t>& image) {
    std::vector<uint8_t> payload = {0};
    const std::string mime = "image/png";
    payload.insert(payload.end(), mime.begin(), mime.end());
    payload.push_back(0);
    payload.push_back(3);
    payload.push_back(0);
    payload.insert(payload.end(), image.begin(), image.end());

    std::vector<uint8_t> frame;
    AppendAscii(frame, "APIC", 4);
    AppendBe32(frame, static_cast<uint32_t>(payload.size()));
    frame.push_back(0);
    frame.push_back(0);
    frame.insert(frame.end(), payload.begin(), payload.end());

    std::vector<uint8_t> bytes;
    AppendAscii(bytes, "ID3", 3);
    bytes.push_back(3);
    bytes.push_back(0);
    bytes.push_back(0);
    AppendSyncSafe32(bytes, static_cast<uint32_t>(frame.size()));
    bytes.insert(bytes.end(), frame.begin(), frame.end());
    return WriteBytes(path, bytes);
}

} // namespace

int main() {
    TempRoot temp;
    const fs::path mp3Path = temp.path / fs::u8path(u8"\u5143\u6570\u636E \u97F3\u6548 \U0001F600.MP3");
    const fs::path wavPath = temp.path / fs::u8path(u8"\u6269\u5C55 \U0001F3A7.WaV");
    const fs::path artPath = temp.path / fs::u8path(u8"\u5C01\u9762 \U0001F5BC space.mP3");
    Check(WriteId3v1(mp3Path, "PathTitle"), "write Unicode ID3v1 fixture");
    Check(WriteBextWav(wavPath, "WaveTitle"), "write Unicode WAV fixture");
    const std::vector<uint8_t> image = {1, 2, 3, 4, 5};
    Check(WriteId3Apic(artPath, image), "write Unicode APIC fixture");

    AudioMetadata mp3;
    Check(MetadataReader::readMetadata(mp3Path.u8string(), mp3) &&
              mp3.title == "PathTitle",
          "readMetadata dispatches mixed-case MP3 on CJK emoji path");
    AudioMetadata wav;
    Check(MetadataReader::readMetadata(wavPath.u8string(), wav) &&
              wav.bwDescription == "WaveTitle",
          "readMetadata dispatches mixed-case WAV on CJK emoji path");
    AlbumArt art;
    Check(MetadataReader::readAlbumArt(artPath.u8string(), art) &&
              art.data == image && art.mimeType == "image/png",
          "readAlbumArt loads APIC on CJK emoji and space path");

    const std::vector<uint8_t> sidecarPng = {9, 8, 7, 6};
    const auto SidecarPath = [&](const char* suffix) {
        fs::path path = mp3Path.parent_path() / mp3Path.stem();
        path += suffix;
        return path;
    };
    const fs::path basePng = SidecarPath(".png");
    const fs::path baseJpeg = SidecarPath(".jpeg");
    const fs::path thumbJpeg = SidecarPath("_thumb.jpg");
    Check(WriteBytes(basePng, sidecarPng), "write Unicode base PNG sidecar");
    Check(WriteBytes(baseJpeg, {1, 1}), "write lower-priority base JPEG sidecar");
    Check(WriteBytes(thumbJpeg, {2, 2}), "write lower-priority thumb sidecar");
    AlbumArt sidecarArt;
    Check(MetadataReader::readAlbumArt(mp3Path.u8string(), sidecarArt) &&
              sidecarArt.data == sidecarPng && sidecarArt.mimeType == "image/png",
          "Unicode sidecar fallback preserves suffix and extension order");

    std::string nulMetadataPath = mp3Path.u8string();
    nulMetadataPath.push_back('\0');
    nulMetadataPath += "shadow.mp3";
    AudioMetadata nulMetadata;
    Check(!MetadataReader::readMetadata(nulMetadataPath, nulMetadata),
          "valid metadata prefix plus embedded NUL fails closed");
    std::string nulArtPath = artPath.u8string();
    nulArtPath.push_back('\0');
    nulArtPath += "shadow.mp3";
    AlbumArt nulArt;
    Check(!MetadataReader::readAlbumArt(nulArtPath, nulArt),
          "valid album-art prefix plus embedded NUL fails closed");

    AudioMetadata missing;
    AlbumArt missingArt;
    const std::string missingPath = (temp.path / "missing.mp3").u8string();
    Check(!MetadataReader::readMetadata(missingPath, missing),
          "missing metadata path fails safely");
    Check(!MetadataReader::readAlbumArt(missingPath, missingArt),
          "missing album-art path fails safely");

    const fs::path corruptPath = temp.path / "malformed.mp3";
    Check(WriteBytes(corruptPath, {0x01, 0x02, 0x03}), "write corrupt fixture");
    AudioMetadata corrupt;
    AlbumArt corruptArt;
    Check(!MetadataReader::readMetadata(corruptPath.u8string(), corrupt),
          "malformed metadata file fails safely");
    Check(!MetadataReader::readAlbumArt(corruptPath.u8string(), corruptArt),
          "malformed album-art file fails safely");

    std::string malformedUtf8 = temp.path.u8string() + "/bad_";
    malformedUtf8.append("\xF0\x28\x8C\x28", 4);
    malformedUtf8 += ".mp3";
    AudioMetadata malformed;
    Check(!MetadataReader::readMetadata(malformedUtf8, malformed),
          "malformed UTF-8 path fails safely");

    const fs::path unknownPath = temp.path / "tagged.bin";
    Check(WriteId3v1(unknownPath, "WrongDispatch"), "write unknown extension fixture");
    AudioMetadata unknown;
    Check(!MetadataReader::readMetadata(unknownPath.u8string(), unknown),
          "unknown extension does not dispatch to ID3 parser");

    std::cout << (failures == 0 ? "PASS" : "FAIL")
              << ": metadata reader path encoding characterization\n";
    return failures == 0 ? 0 : 1;
}
