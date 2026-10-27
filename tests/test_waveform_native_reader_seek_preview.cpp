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

static std::string body(const std::string& text, const char* signature) {
    size_t start = text.find(signature);
    while (start != std::string::npos) {
        const size_t semi = text.find(';', start);
        const size_t open = text.find('{', start);
        if (open != std::string::npos && (semi == std::string::npos || open < semi)) {
            int depth = 0;
            for (size_t i = open; i < text.size(); ++i) {
                if (text[i] == '{') ++depth;
                else if (text[i] == '}' && --depth == 0)
                    return text.substr(open, i - open + 1);
            }
            return {};
        }
        start = text.find(signature, start + 1);
    }
    return {};
}

static void check(bool ok, const char* label, int& failures) {
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

int main() {
    const std::string text = source();
    const std::string seek = body(text, "static bool ExtractMiniSeekBased(const std::string& filePath,");
    const std::string preview = body(text, "bool WaveformExtractor::ExtractMiniPreview(const std::string& filePath,");
    int failures = 0;
    check(!seek.empty() && !preview.empty(), "seek and preview bodies are inspectable", failures);
    check(seek.find("NativeFloatReader") != std::string::npos,
          "seek lane uses NativeFloatReader", failures);
    check(seek.find("seek(") != std::string::npos,
          "seek lane delegates frame seeks", failures);
    check(seek.find("0.0f") != std::string::npos && seek.find("continue") != std::string::npos,
          "seek failures retain zero-filled buckets", failures);
    check(seek.find("cancelFlag") != std::string::npos && seek.find("cancelled") != std::string::npos,
          "seek lane retains cancellation classification", failures);
    check(preview.find("NativeFloatReader") != std::string::npos,
          "preview lane uses NativeFloatReader", failures);
    check(preview.find("maxDecodeSeconds") != std::string::npos,
          "preview lane retains decode-time bound", failures);
    check(preview.find("ReadStatus::Error") != std::string::npos,
          "preview lane observes partial terminal errors", failures);
    return failures == 0 ? 0 : 1;
}
