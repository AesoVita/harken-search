#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
static std::string source() {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream in(repoRoot / "src/WaveformExtractor.cpp", std::ios::binary);
    if (!in) return {};
    return std::string((std::istreambuf_iterator<char>(in)), {});
}
static std::string body(const std::string& s, const char* sig) {
    size_t start = s.find(sig);
    while (start != std::string::npos) {
        size_t semi = s.find(';', start), open = s.find('{', start);
        if (open != std::string::npos && (semi == std::string::npos || open < semi)) {
            int d = 0; for (size_t i = open; i < s.size(); ++i) {
                if (s[i] == '{') ++d; else if (s[i] == '}' && --d == 0) return s.substr(open, i - open + 1);
            }
            return {};
        }
        start = s.find(sig, start + 1);
    }
    return {};
}
static void check(bool ok, const char* label, int& failures) { if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); } }
int main() {
    const std::string s = source();
    const std::string probe = body(s, "bool WaveformExtractor::ExtractMiniReturningDecoded(");
    const std::string rms = body(s, "float WaveformExtractor::ComputeTrueRmsDbfs(const std::string& filePath)");
    int failures = 0;
    check(!probe.empty() && !rms.empty(), "probe and RMS bodies are inspectable", failures);
    check(probe.find("NativeFloatReader") != std::string::npos, "native probe uses reader", failures);
    check(probe.find("cancelFlag") != std::string::npos && probe.find("cancelled") != std::string::npos,
          "probe retains cancellation", failures);
    check(probe.find("sampleRate") != std::string::npos && probe.find("totalFrames") != std::string::npos,
          "probe retains native rate and frame count", failures);
    check(rms.find("NativeFloatReader") != std::string::npos, "true RMS uses reader", failures);
    check(rms.find("sumSq") != std::string::npos && rms.find("totalSamples") != std::string::npos,
          "RMS remains channel-independent sum of squares", failures);
    check(rms.find("ReadStatus::Error") != std::string::npos,
          "RMS retains terminal error semantics", failures);
    return failures == 0 ? 0 : 1;
}
