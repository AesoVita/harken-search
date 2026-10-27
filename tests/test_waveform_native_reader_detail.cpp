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
            int depth = 0;
            for (size_t i = open; i < s.size(); ++i) {
                if (s[i] == '{') ++depth;
                else if (s[i] == '}' && --depth == 0) return s.substr(open, i - open + 1);
            }
            return {};
        }
        start = s.find(sig, start + 1);
    }
    return {};
}
static void check(bool ok, const char* label, int& failures) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
int main() {
    const std::string s = source();
    const std::string seq = body(s, "static bool ExtractDetailSequential(const std::string& filePath,");
    const std::string two = body(s, "static bool ExtractDetailTwoPassSampleAccurate(const std::string& filePath,");
    const std::string one = body(s, "static bool ExtractDetailSampleAccurate(const std::string& filePath,");
    int failures = 0;
    check(!seq.empty() && !two.empty() && !one.empty(), "all detail bodies are inspectable", failures);
    for (const auto* b : {&seq, &two, &one}) {
        check(b->find("NativeFloatReader") != std::string::npos, "detail lane uses NativeFloatReader", failures);
        check(b->find("cancelFlag") != std::string::npos && b->find("cancelled") != std::string::npos,
              "detail lane retains cancellation", failures);
        check(b->find("decodedFrames") != std::string::npos, "detail lane tracks decoded frames", failures);
        check(b->find("reportedFrames") != std::string::npos || b->find("hasReportedFrames") != std::string::npos,
              "detail lane retains reported length hint", failures);
    }
    check(one.find("kSinglePassSampleCapBytes") != std::string::npos, "single-pass cap remains", failures);
    check(one.find("ExtractDetailTwoPassSampleAccurate") != std::string::npos, "cap fallback remains", failures);
    check(two.find("reader.open(filePath)") != std::string::npos, "two-pass lane reopens reader", failures);
    check(s.find("ma_decoder_") == std::string::npos,
          "waveform extractor has no direct decoder ownership", failures);
    return failures == 0 ? 0 : 1;
}
