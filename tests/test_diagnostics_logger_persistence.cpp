#include "DiagnosticsLogger.h"
#include "platform/thread_id.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <thread>

static std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "aris_stage6_diagnostics_pal";
    const fs::path livePath = root / "live.jsonl";
    const fs::path exportPath = root / "export.json";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    auto& diag = DiagnosticsLogger::instance();
    const uint32_t expectedThreadId = Platform::CurrentThreadId();
    diag.setOutputPath(livePath.u8string());
    diag.setEnabled(true);
    diag.logSystem("Stage6", "stream writer probe");
    const std::string oversizedDetail(191, 'x');
    diag.logPerformance("CrashOverflow", oversizedDetail.c_str(),
                        std::numeric_limits<double>::max());
    diag.logSystem("CrashSafe", "after oversized record");
    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    diag.setEnabled(false);

    const std::string live = ReadAll(livePath);
    if (live.find("\"category\":\"Stage6\"") == std::string::npos ||
        live.find("stream writer probe") == std::string::npos ||
        live.find("\"threadId\":" + std::to_string(expectedThreadId)) == std::string::npos) {
        std::cerr << "FAIL: diagnostics stream did not flush JSONL\n";
        return 1;
    }
    if (!diag.exportJson(exportPath.u8string())) {
        std::cerr << "FAIL: diagnostics JSON export failed\n";
        return 1;
    }
    const std::string exported = ReadAll(exportPath);
    if (exported.find("\"logs\"") == std::string::npos) {
        std::cerr << "FAIL: diagnostics JSON export content missing\n";
        return 1;
    }

    const fs::path crashPath = root / "crash.jsonl";
#ifdef _WIN32
    if (!diag.exportJsonLinesCrashSafe(crashPath.c_str())) {
        std::cerr << "FAIL: crash-safe JSONL export failed\n";
        return 1;
    }
    const std::string crash = ReadAll(crashPath);
    if (crash.find("\"category\":\"CrashSafe\"") == std::string::npos ||
        crash.find("\"category\":\"Stage6\"") == std::string::npos ||
        crash.find("CrashOverflow") != std::string::npos) {
        std::cerr << "FAIL: crash-safe export did not continue after the oversized record\n";
        return 1;
    }
#else
    if (diag.exportJsonLinesCrashSafe(L"unsupported")) {
        std::cerr << "FAIL: portable crash-safe export should be unsupported\n";
        return 1;
    }
#endif

    fs::remove_all(root, ec);
    std::cout << "PASS: diagnostics normal output uses filesystem PAL\n";
    return 0;
}
