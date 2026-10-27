#include "NamingScript.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

struct TempDirectory {
    fs::path path;

    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("aris-naming-conflict-" + std::to_string(stamp));
        fs::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

void Touch(const fs::path& path) {
    std::ofstream stream(path, std::ios::binary);
    stream.put('x');
}

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

} // namespace

int main() {
    TempDirectory temp;
    const std::string root = temp.path.u8string();

    Touch(temp.path / "take.wav");
    if (!Check(NamingScriptEngine::resolveConflict(root, "take", ".wav") == "take_1.wav",
               "existing file should select _1")) return 1;

    Touch(temp.path / "take_1.wav");
    fs::create_directory(temp.path / "take_2.wav");
    if (!Check(NamingScriptEngine::resolveConflict(root, "take", ".wav") == "take_3.wav",
               "file and directory conflicts should preserve suffix order")) return 1;
    if (!Check(NamingScriptEngine::resolveConflict(root, "take", ".wav", 2).empty(),
               "exhausted maxAttempts should return empty")) return 1;
    if (!Check(NamingScriptEngine::resolveConflict(root, "take", ".wav", 0).empty(),
               "non-positive maxAttempts should return empty after initial conflict")) return 1;

    const std::string unicodeName = u8"中文 空格";
    Touch(temp.path / fs::u8path(unicodeName + ".wav"));
    if (!Check(NamingScriptEngine::resolveConflict(root, unicodeName, ".wav") ==
                   unicodeName + "_1.wav",
               "Unicode and spaces should remain unchanged")) return 1;

    if (!Check(NamingScriptEngine::resolveConflict(root, "fresh", ".flac", 0) == "fresh.flac",
               "initial missing candidate should not consume maxAttempts")) return 1;

    const std::unordered_map<std::string, std::string> metadata = {
        {"Description", "Big Boom"}, {"Source", "Field Recorder"},
    };
    if (!Check(NamingScriptEngine::applyTemplate(
                   "<Description:1>_<Source:2>", metadata, 255) == "Big_Field Recorder",
               "template output changed")) return 1;
    if (!Check(NamingScriptEngine::sanitizeFilename("  bad:name?.wav. ") == "bad_name_.wav",
               "sanitize output changed")) return 1;
    if (!Check(NamingScriptEngine::truncateToLength(u8"音效ABC", 3) == u8"音效A",
               "UTF-8 truncation output changed")) return 1;

    std::cout << "PASS: NamingScript conflict and pure output contracts\n";
    return 0;
}
