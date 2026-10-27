#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static std::string readSource() {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    std::ifstream in(repoRoot / "src/WaveformExtractor.cpp", std::ios::binary);
    if (!in) return {};
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

static std::string functionBody(const std::string& source, const char* signature) {
    size_t start = source.find(signature);
    size_t open = std::string::npos;
    while (start != std::string::npos) {
        const size_t nextSemi = source.find(';', start);
        const size_t candidate = source.find('{', start);
        if (candidate != std::string::npos &&
            (nextSemi == std::string::npos || candidate < nextSemi)) {
            open = candidate;
            break;
        }
        start = source.find(signature, start + 1);
    }
    if (open == std::string::npos) return {};
    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{') ++depth;
        else if (source[i] == '}' && --depth == 0)
            return source.substr(open, i - open + 1);
    }
    return {};
}

static void check(bool condition, const char* label, int& failures) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

int main() {
    const std::string source = readSource();
    int failures = 0;
    check(!source.empty(), "WaveformExtractor source is readable", failures);

    const std::string known = functionBody(
        source, "static bool ExtractMiniSequential(const std::string& filePath,");
    const std::string unknown = functionBody(
        source, "static bool ExtractMiniSequentialUnknownLength(const std::string& filePath,");
    check(!known.empty() && !unknown.empty(), "both mini sequential bodies are inspectable", failures);

    for (const auto* body : {&known, &unknown}) {
        check(body->find("NativeFloatReader") != std::string::npos,
              "mini sequential lane owns decode through NativeFloatReader", failures);
        check(body->find("cancelFlag") != std::string::npos,
              "mini sequential lane retains cancellation input", failures);
        check(body->find("errorCode") != std::string::npos,
              "mini sequential lane retains error classification", failures);
        check(body->find("samplesPerPeak") != std::string::npos,
              "mini sequential lane retains bucket sizing", failures);
    }
    check(source.find("reader.metadata().reportedFrames") != std::string::npos ||
              source.find("reader.metadata().hasReportedFrames") != std::string::npos,
          "known-length lane treats reported length as a hint", failures);
    check(unknown.find("open(filePath)") != std::string::npos,
          "unknown-length lane reopens reader for second pass", failures);
    check(unknown.find("decodedFrames") != std::string::npos,
          "unknown-length lane derives density from decoded frames", failures);
    check(source.find("ma_decoder_") == std::string::npos,
          "waveform extractor has no direct decoder ownership", failures);
    return failures == 0 ? 0 : 1;
}
