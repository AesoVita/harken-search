#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string ReadRepoFile(const char* relativePath)
{
    std::filesystem::path here(__FILE__);
    std::filesystem::path root = here.parent_path().parent_path();
    std::ifstream in(root / relativePath, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string ExtractFunctionBody(const std::string& source, const std::string& signature)
{
    const size_t sigPos = source.find(signature);
    if (sigPos == std::string::npos) return {};
    const size_t open = source.find('{', sigPos);
    if (open == std::string::npos) return {};

    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{') {
            ++depth;
        } else if (source[i] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(open, i - open + 1);
            }
        }
    }
    return {};
}

bool ContainsInOrder(const std::string& text,
                     const std::string& first,
                     const std::string& second,
                     const std::string& third)
{
    const size_t a = text.find(first);
    if (a == std::string::npos) return false;
    const size_t b = text.find(second, a + first.size());
    if (b == std::string::npos) return false;
    const size_t c = text.find(third, b + second.size());
    return c != std::string::npos;
}

} // namespace

int main()
{
    int failed = 0;

    const std::string audio = ReadRepoFile("src/AudioEngine.cpp");
    if (audio.empty()) {
        std::printf("FAIL: src/AudioEngine.cpp unreadable\n");
        return 1;
    }

    const std::string body = ExtractFunctionBody(audio, "bool AudioEngine::reloadWithReverseState()");
    if (body.empty()) {
        std::printf("FAIL: reloadWithReverseState body not found\n");
        return 1;
    }

    if (body.find("std::unique_lock<std::mutex> lock(m_mutex);") == std::string::npos) {
        std::printf("FAIL: reverse reload must use unique_lock so the decoder reload can temporarily unlock safely\n");
        ++failed;
    }

    if (!ContainsInOrder(body, "lock.unlock();", "bool loaded = loadReversedAudio();", "lock.lock();")) {
        std::printf("FAIL: reverse reload must unlock/relock the same unique_lock around loadReversedAudio\n");
        ++failed;
    }

    if (body.find("m_mutex.unlock();") != std::string::npos) {
        std::printf("FAIL: reverse reload must not manually unlock m_mutex while a scoped lock object exists\n");
        ++failed;
    }

    if (!ContainsInOrder(body,
                         "if (!loaded || m_reverseBuffer.empty())",
                         "restoreForwardAfterReverseFailure = true",
                         "if (restoreForwardAfterReverseFailure)")) {
        std::printf("FAIL: reverse decode failure must set restoreForwardAfterReverseFailure and continue\n");
        ++failed;
    }

    const size_t failPos = body.find("if (!loaded || m_reverseBuffer.empty())");
    const size_t pcmPos = body.find("InitializePcmBuffer");
    if (failPos != std::string::npos && pcmPos != std::string::npos && pcmPos > failPos) {
        const std::string failBranch = body.substr(failPos, pcmPos - failPos);
        if (failBranch.find("return false;") != std::string::npos) {
        std::printf("FAIL: reverse decode failure must not return false before restore\n");
        ++failed;
        }
        if (failBranch.find("restoreForwardAfterReverseFailure = true") == std::string::npos) {
        std::printf("FAIL: empty reverse buffer must set restoreForwardAfterReverseFailure\n");
        ++failed;
        }
    }

    if (!ContainsInOrder(body,
                         "if (restoreForwardAfterReverseFailure)",
                         "installForwardSound",
                         "return !restoreForwardAfterReverseFailure;")) {
        std::printf("FAIL: restore flag must call installForwardSound and still return failure\n");
        ++failed;
    }

    std::string reverseBody = ExtractFunctionBody(audio, "bool AudioEngine::setReverse(bool reverse)");
    if (reverseBody.empty()) {
        reverseBody = ExtractFunctionBody(audio, "void AudioEngine::setReverse(bool reverse)");
    }
    if (reverseBody.find("originalPos") == std::string::npos ||
        reverseBody.find("reloaded ? pos : originalPos") == std::string::npos) {
        std::printf("FAIL: setReverse must seek originalPos when reload reports failure\n");
        ++failed;
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
