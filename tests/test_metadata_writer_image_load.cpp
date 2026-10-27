#include "MetadataWriter.h"

#include <chrono>
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
        ("aris_metadata_writer_image_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    TempRoot() { fs::create_directories(path); }
    ~TempRoot() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

bool WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

std::string ReadSource() {
    const fs::path sourcePath = fs::path(AURA_TEST_SOURCE_ROOT) / "src" / "MetadataWriter.cpp";
    std::ifstream input(sourcePath, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    const std::string source = ReadSource();
    const size_t begin = source.find("bool MetadataWriter::loadImageFile");
    const size_t end = source.find("bool MetadataWriter::writeAlbumArt", begin);
    Check(begin != std::string::npos && end != std::string::npos,
          "loadImageFile source boundary should be discoverable");
    if (begin != std::string::npos && end != std::string::npos) {
        const std::string body = source.substr(begin, end - begin);
        Check(body.find("Platform::FileSystem::StreamReader") != std::string::npos,
              "loadImageFile should use the filesystem stream PAL");
        Check(body.find("std::ifstream") == std::string::npos,
              "loadImageFile should not construct native input streams");
    }

    TempRoot root;
    std::vector<uint8_t> imageData{0x7F};
    std::string mimeType = "unchanged";
    Check(!MetadataWriter::loadImageFile((root.path / "missing.jpg").u8string(),
                                         imageData, mimeType),
          "missing image should fail to load");

    const fs::path emptyPath = root.path / "empty.bin";
    Check(WriteBytes(emptyPath, {}), "empty fixture should be writable");
    Check(MetadataWriter::loadImageFile(emptyPath.u8string(), imageData, mimeType),
          "empty image should preserve existing success behavior");
    Check(imageData.empty(), "empty image should return no bytes");
    Check(mimeType == "image/jpeg", "empty image should use the JPEG fallback MIME");

    const std::vector<uint8_t> jpeg{0xFF, 0xD8, 0xFF, 0x12, 0x34};
    const fs::path jpegPath = root.path / fs::u8path("image-\xE5\x85\x83\xE6\x95\xB0\xE6\x8D\xAE.jpg");
    Check(WriteBytes(jpegPath, jpeg), "JPEG fixture should be writable");
    Check(MetadataWriter::loadImageFile(jpegPath.u8string(), imageData, mimeType),
          "UTF-8 JPEG path should load");
    Check(imageData == jpeg, "JPEG bytes should be preserved exactly");
    Check(mimeType == "image/jpeg", "JPEG signature should select JPEG MIME");

    const std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const fs::path pngPath = root.path / "image.png";
    Check(WriteBytes(pngPath, png), "PNG fixture should be writable");
    Check(MetadataWriter::loadImageFile(pngPath.u8string(), imageData, mimeType),
          "PNG image should load");
    Check(imageData == png, "PNG bytes should be preserved exactly");
    Check(mimeType == "image/png", "PNG signature should select PNG MIME");

    const std::vector<uint8_t> unknown{'G', 'I', 'F'};
    const fs::path unknownPath = root.path / "image.gif";
    Check(WriteBytes(unknownPath, unknown), "unknown fixture should be writable");
    Check(MetadataWriter::loadImageFile(unknownPath.u8string(), imageData, mimeType),
          "unknown image should load");
    Check(imageData == unknown, "unknown image bytes should be preserved exactly");
    Check(mimeType == "image/jpeg", "unknown signature should retain JPEG fallback MIME");

    if (failures != 0) return 1;
    std::cout << "PASS: metadata image loading preserves bytes, MIME, and UTF-8 paths\n";
    return 0;
}
