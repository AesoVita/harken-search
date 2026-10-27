#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>
#include <string>

int main() {
    const std::filesystem::path candidates[] = {
        std::filesystem::current_path() / "src/main.cpp",
        std::filesystem::current_path() / ".." / "src/main.cpp",
        std::filesystem::path(__FILE__).parent_path().parent_path() / "src/main.cpp",
    };
    std::ifstream in;
    for (const auto& candidate : candidates) {
        in.open(candidate, std::ios::binary);
        if (in) break;
        in.clear();
    }
    if (!in) {
        std::cerr << "unable to read src/main.cpp\n";
        return 1;
    }
    const std::string source((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
    const bool hasPalHelper = source.find("AppendHarkenSearchPerfLogLine") != std::string::npos;
    const bool usesTempDirectory =
        source.find("Platform::FileSystem::TemporaryDirectory()") != std::string::npos;
    const bool stillHardcodesWindowsTemp = source.find("C:\\\\tmp\\\\auro_perf.log") != std::string::npos;
    const bool stillUsesCFileApi = source.find("fopen_s(&logFile") != std::string::npos;
    if (!hasPalHelper || !usesTempDirectory || stillHardcodesWindowsTemp || stillUsesCFileApi) {
        std::cerr << "main.cpp performance log must use PAL-owned temp path append helper\n";
        return 1;
    }
    return 0;
}
