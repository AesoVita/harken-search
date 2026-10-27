#include "AudioEnvironmentPolicy.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

int failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

Platform::EnvironmentStringResult Result(
    Platform::EnvironmentStringStatus status, std::string value = {}) {
    Platform::EnvironmentStringResult result;
    result.status = status;
    result.valueUtf8 = std::move(value);
    return result;
}

std::string ReadAudioEngineSource() {
    std::ifstream input(AURO_AUDIO_ENGINE_SOURCE, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

int main() {
    using Platform::EnvironmentStringStatus;
    using HarkenSearch::AudioEnvironment::IsEnabled;

    Check(!IsEnabled(Result(EnvironmentStringStatus::NotFound)),
          "missing environment flags are disabled");
    Check(!IsEnabled(Result(EnvironmentStringStatus::QueryFailed, "1")),
          "query failures are disabled even when a value is present");
    Check(!IsEnabled(Result(EnvironmentStringStatus::EncodingFailed, "1")),
          "encoding failures are disabled");
    Check(!IsEnabled(Result(EnvironmentStringStatus::ValueTooLarge, "1")),
          "oversized values are disabled");
    Check(!IsEnabled(Result(EnvironmentStringStatus::Success)),
          "successful empty values are disabled");
    Check(!IsEnabled(Result(EnvironmentStringStatus::Success, "0")),
          "values beginning with zero are disabled");
    Check(!IsEnabled(Result(EnvironmentStringStatus::Success, "0enabled")),
          "the first character controls zero-prefixed values");
    Check(IsEnabled(Result(EnvironmentStringStatus::Success, "1")),
          "successful nonzero values are enabled");
    Check(IsEnabled(Result(EnvironmentStringStatus::Success, "enabled")),
          "successful nonempty text values are enabled");

    const std::string source = ReadAudioEngineSource();
    Check(!source.empty(), "AudioEngine.cpp is readable by the compiled contract");
    Check(source.find("GetEnvironmentVariableA") == std::string::npos,
          "AudioEngine does not query environment variables through Win32");
    Check(source.find("#include <windows.h>") == std::string::npos,
          "AudioEngine does not include the Win32 SDK directly");
    Check(source.find("Platform::ReadEnvironmentVariable") != std::string::npos,
          "AudioEngine reads environment variables through the PAL");

    return failures == 0 ? 0 : 1;
}
