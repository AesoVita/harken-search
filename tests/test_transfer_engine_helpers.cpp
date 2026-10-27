#include "src/TransferEngine.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int failed = 0;

static void check(bool cond, const char* msg) {
    if (cond) {
        std::cout << "PASS: " << msg << "\n";
    } else {
        std::cout << "FAIL: " << msg << "\n";
        ++failed;
    }
}

static fs::path makeTempDir() {
    fs::path dir = fs::temp_directory_path() /
        ("auro_transfer_helpers_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    return dir;
}

static void writeTinyFile(const fs::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

static void testExistingDestinationResolution() {
    fs::path dir = makeTempDir();
    fs::path requested = dir / "hit.wav";
    writeTinyFile(requested);

    auto skip = TransferEngine::resolveExistingDestinationPath(
        requested.u8string(),
        ExistingFileBehavior::SkipExisting);
    check(!skip.shouldCopy && skip.skipped && skip.path == requested.u8string(),
          "SkipExisting marks existing destination as skipped");

    auto replace = TransferEngine::resolveExistingDestinationPath(
        requested.u8string(),
        ExistingFileBehavior::ReplaceExisting);
    check(replace.shouldCopy && replace.wouldOverwrite && replace.path == requested.u8string(),
          "ReplaceExisting keeps requested path and marks overwrite");

    auto createNew = TransferEngine::resolveExistingDestinationPath(
        requested.u8string(),
        ExistingFileBehavior::AlwaysCreateNew);
    check(createNew.shouldCopy && !createNew.skipped &&
              createNew.path == (dir / "hit_1.wav").u8string(),
          "AlwaysCreateNew picks first free numeric suffix");

    writeTinyFile(dir / "hit_1.wav");
    auto exhausted = TransferEngine::resolveExistingDestinationPath(
        requested.u8string(),
        ExistingFileBehavior::AlwaysCreateNew,
        1);
    check(!exhausted.shouldCopy && !exhausted.errorMessage.empty(),
          "AlwaysCreateNew reports suffix exhaustion");

    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void testSafeDestinationFileName() {
    TransferPrefs prefs;
    prefs.stripIllegals = true;
    prefs.filenameLimit = 12;

    check(TransferEngine::buildSafeDestinationFileName("bad:name?.wav", prefs) == "bad_name.wav",
          "safe file name strips illegal characters and trims to limit");

    prefs.filenameLimit = 255;
    check(TransferEngine::buildSafeDestinationFileName("   .wav", prefs) == "untitled.wav",
          "safe file name falls back when stem trims empty");

    prefs.stripIllegals = false;
    check(TransferEngine::buildSafeDestinationFileName("bad:name?.wav", prefs) == "bad:name?.wav",
          "safe file name can preserve source name when stripping disabled");
}

static void testSingleChannelExportHelpers() {
    check(!TransferEngine::canExtractSingleChannel(1),
          "single-channel extraction is disabled for mono files");
    check(!TransferEngine::canExtractSingleChannel(2),
          "single-channel extraction is disabled for stereo files");
    check(TransferEngine::canExtractSingleChannel(6),
          "single-channel extraction is enabled for surround files");

    check(TransferEngine::getSingleChannelExportLabel(0, 6) == "L",
          "surround channel 1 is labeled L");
    check(TransferEngine::getSingleChannelExportLabel(2, 6) == "C",
          "surround channel 3 is labeled C");
    check(TransferEngine::getSingleChannelExportLabel(3, 6) == "LFE",
          "surround channel 4 is labeled LFE");
    check(TransferEngine::getSingleChannelExportLabel(5, 6) == "Rs",
          "surround channel 6 is labeled Rs");
    check(TransferEngine::getSingleChannelExportLabel(8, 10) == "Ch9",
          "unknown extended surround channels fall back to one-based labels");

    TransferPrefs prefs;
    prefs.stripIllegals = true;
    prefs.filenameLimit = 255;
    check(TransferEngine::buildSingleChannelExportFileName(
              "C:\\Library\\Impact Take.aif", 3, 6, prefs) == "Impact Take_LFE.wav",
          "single-channel export names use source stem, channel label, and WAV extension");

    prefs.filenameLimit = 18;
    check(TransferEngine::buildSingleChannelExportFileName(
              "bad:scene?.wav", 3, 6, prefs) == "bad_scene__LFE.wav",
          "single-channel export names keep channel label after illegal cleanup and trimming");
}

int main() {
    testExistingDestinationResolution();
    testSafeDestinationFileName();
    testSingleChannelExportHelpers();

    if (failed != 0) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "All transfer helper tests passed\n";
    return 0;
}
