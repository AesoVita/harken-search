#include "platform/filesystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool WriteText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
    return output.good();
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    std::error_code ec;
    const auto uniqueSuffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(ec) / ("aris-atomic-replace-" + uniqueSuffix);
    if (ec || !std::filesystem::create_directories(root, ec) || ec) {
        std::cerr << "FAIL: could not create atomic replace fixture\n";
        return 1;
    }

    const std::filesystem::path target = root / "target.txt";
    const std::filesystem::path source = root / "source.tmp";
    if (!WriteText(target, "original") || !WriteText(source, "replacement")) {
        std::cerr << "FAIL: could not write atomic replace fixture\n";
        std::filesystem::remove_all(root, ec);
        return 1;
    }

    if (!Platform::FileSystem::AtomicFileReplace(target.u8string(), source.u8string()) ||
        ReadText(target) != "replacement" || std::filesystem::exists(source)) {
        std::cerr << "FAIL: successful atomic replace did not consume source and replace target\n";
        std::filesystem::remove_all(root, ec);
        return 1;
    }

    if (!WriteText(target, "preserve-me")) {
        std::cerr << "FAIL: could not reset atomic replace target\n";
        std::filesystem::remove_all(root, ec);
        return 1;
    }
    const std::filesystem::path missingSource = root / "missing.tmp";
    const bool replacedMissingSource =
        Platform::FileSystem::AtomicFileReplace(target.u8string(), missingSource.u8string());
    if (replacedMissingSource || !std::filesystem::exists(target) ||
        ReadText(target) != "preserve-me") {
        std::cerr << "FAIL: failed atomic replace must preserve the existing target\n";
        std::filesystem::remove_all(root, ec);
        return 1;
    }

    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: atomic file replace preserves target on failure\n";
    return 0;
}
