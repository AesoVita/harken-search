#include "platform/file_hash.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    const fs::path root = fs::temp_directory_path() / "aris-file-hash-contract";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path empty = root / "empty.bin";
    const fs::path abc = root / "abc.bin";
    std::ofstream(empty, std::ios::binary);
    std::ofstream(abc, std::ios::binary) << "abc";

    const auto emptyResult = Platform::Crypto::ComputeSha256File(empty.u8string());
    const auto abcResult = Platform::Crypto::ComputeSha256File(abc.u8string());
    const auto missing = Platform::Crypto::ComputeSha256File((root / "missing.bin").u8string());
    fs::remove_all(root);

    if (emptyResult.status != Platform::Crypto::FileHashStatus::Success ||
        emptyResult.hexDigest !=
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") {
        std::cerr << "empty SHA-256 mismatch\n";
        return 1;
    }
    if (abcResult.status != Platform::Crypto::FileHashStatus::Success ||
        abcResult.hexDigest !=
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        std::cerr << "abc SHA-256 mismatch\n";
        return 1;
    }
    if (missing.status != Platform::Crypto::FileHashStatus::OpenFailed ||
        !missing.hexDigest.empty()) {
        std::cerr << "missing-file status mismatch\n";
        return 1;
    }
    return 0;
}
