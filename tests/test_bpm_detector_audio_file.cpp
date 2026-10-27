#include "BpmDetector.h"
#include "audio_file_reader_scripted.h"

#include <cstdio>
#include <vector>

using namespace AudioFileScripted;
using Platform::AudioFile::FrameCount;
using Platform::AudioFile::LegacyWindowsPathPolicy;
using Platform::AudioFile::MonoSampleRate;
using Platform::AudioFile::Status;

namespace {
int failures = 0;
void Check(bool ok, const char* text) {
    if (!ok) { std::printf("FAIL: %s\n", text); ++failures; }
}
void IsDefault(const BpmResult& result, const char* text) {
    Check(!result.ok && result.bpm == 0.0f && result.confidence == 0.0f &&
              result.beatOffset == 0.0f, text);
}
}

int main() {
    Reset();
    QueueOpen(Status::Ok);
    QueueSeek(Status::SeekFailed);
    QueueRead({Status::DecodeFailed, 44100}, std::vector<float>(44100, 0.5f));
    const auto detected = BpmDetector::detectFile("beat.wav", 1.25, 4.0, 1.5);
    const auto& state = Get();
    Check(detected.ok && detected.bpm == 120.0f && detected.confidence == 0.75f,
          "bpm: positive hard-status read still reaches detector");
    Check(state.calls == std::vector<std::string>{"Open", "Seek", "Read", "Close"},
          "bpm: open seek read close order");
    Check(state.paths == std::vector<std::string>{"beat.wav"} &&
              state.policies == std::vector<LegacyWindowsPathPolicy>{
                  LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix} &&
              state.sampleRates == std::vector<MonoSampleRate>{MonoSampleRate::Hz44100},
          "bpm: path policy and 44100 output are preserved");
    Check(state.seeks == std::vector<FrameCount>{55125} &&
              state.readRequests == std::vector<FrameCount>{66150},
          "bpm: legacy seek and capped duration frame math");

    Reset(); QueueOpen(Status::OpenFailed);
    IsDefault(BpmDetector::detectFile("missing.wav"), "bpm: open failure returns default");
    Check(Get().calls == std::vector<std::string>{"Open"},
          "bpm: open failure avoids seek and read");

    Reset(); QueueOpen(Status::Ok);
    QueueRead({Status::EndOfFile, 44099}, std::vector<float>(44099, 0.5f));
    IsDefault(BpmDetector::detectFile("short.wav", 0.0, -1.0, 1.0),
              "bpm: less than one second remains rejected");
    return failures ? 1 : 0;
}
