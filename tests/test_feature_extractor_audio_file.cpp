#include "audio_file_reader_scripted.h"
#include "FeatureExtractor.h"
#include <algorithm>
#include <array>
#include <cstdio>

using namespace Platform::AudioFile;
using namespace AudioFileScripted;
namespace {
int failures = 0;
void Check(bool ok, const char* text) { if (!ok) { std::printf("FAIL: %s\n", text); ++failures; } }
}
int main() {
    Reset();
    QueueOpen(Status::Ok);
    QueueSeek(Status::SeekFailed);
    QueueRead({Status::DecodeFailed, 2}, {0.25f, -0.5f, 9.0f});
    MonoFloatReader reader;
    Check(reader.Open("script.wav", LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix) == Status::Ok,
          "script: queued open");
    Check(reader.Seek(42) == Status::SeekFailed, "script: queued seek");
    float output[2] = {};
    const auto read = reader.Read(output, 2);
    Check(read.status == Status::DecodeFailed && read.framesRead == 2 &&
          output[0] == 0.25f && output[1] == -0.5f, "script: bounded partial hard read");
    Check(Get().paths == std::vector<std::string>{"script.wav"} &&
          Get().policies == std::vector<LegacyWindowsPathPolicy>{LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix},
          "script: path and policy recorded");
    Check(Get().seeks == std::vector<FrameCount>{42} && Get().readRequests == std::vector<FrameCount>{2},
          "script: seek and request recorded");
    MonoFloatReader moved(std::move(reader));
    Check(!reader.IsOpen() && moved.IsOpen(), "script: move construction");
    MonoFloatReader assigned;
    assigned = std::move(moved);
    Check(!moved.IsOpen() && assigned.IsOpen(), "script: move assignment");
    assigned.Close(); assigned.Close();
    Check(!assigned.IsOpen() && Get().calls == std::vector<std::string>{"Open","Seek","Read","Close"},
          "script: ordered idempotent close");
    Reset();
    Check(Get().calls.empty() && Get().openQueue.empty() && Get().seekQueue.empty() &&
          Get().readQueue.empty() && Get().paths.empty() && Get().policies.empty() &&
          Get().seeks.empty() && Get().readRequests.empty(), "script: reset clears state");
    MonoFloatReader defaults;
    float untouched = 7.0f;
    Check(defaults.Open("none", LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix) == Status::OpenFailed &&
          defaults.Read(&untouched, 1).status == Status::InvalidArgument && untouched == 7.0f,
          "script: deterministic empty defaults");
    Reset();
    QueueOpen(Status::Ok);
    QueueRead({Status::DecodeFailed, 4}, {1.0f, 2.0f, 3.0f});
    MonoFloatReader bounded;
    bounded.Open("bounded.wav", LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix);
    std::array<float, 4> guarded = {-1.0f, -1.0f, 99.0f, 99.0f};
    const auto capped = bounded.Read(guarded.data(), 2);
    Check(capped.status == Status::DecodeFailed && capped.framesRead == 2 &&
          guarded[0] == 1.0f && guarded[1] == 2.0f && guarded[2] == 99.0f && guarded[3] == 99.0f,
          "script: over-reported read is capped without overwrite");
    bounded.Close();
    Check(!bounded.IsOpen(), "script: bounded reader closes before reset");
    Reset();
    QueueOpen(Status::Ok); QueueOpen(Status::Ok);
    MonoFloatReader source, destination;
    source.Open("source.wav", LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix);
    destination.Open("destination.wav", LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix);
    destination = std::move(source);
    Check(!source.IsOpen() && destination.IsOpen() &&
          Get().calls == std::vector<std::string>{"Open", "Open", "Close"},
          "script: move assignment closes existing destination first");
    destination.Close();
    Check(Get().calls == std::vector<std::string>{"Open", "Open", "Close", "Close"},
          "script: moved destination closes once");
    Reset();
    QueueOpen(Status::Ok);
    QueueRead({Status::DecodeFailed, 2}, {0.1f, 0.2f});
    QueueRead({Status::EndOfFile, 0});
    const auto failedRead = FeatureExtractor::extractFromFileChecked("feature.wav");
    Check(!failedRead.ok && failedRead.error == FeatureExtractionError::DecodeFailed,
          "feature: partial hard decode failure is not published as valid features");
    Check(Get().calls == std::vector<std::string>{"Open", "Read", "Close"},
          "feature: hard decode failure closes without another read");
    Check(Get().paths[0] == "feature.wav" && Get().policies[0] ==
          LegacyWindowsPathPolicy::FeatureLegacyUtf8VerbatimPrefix,
          "feature: exact path and policy");
    Check(Get().readRequests == std::vector<FrameCount>{8192},
          "feature: hard decode failure stops after the first chunk");
    Reset();
    QueueOpen(Status::OpenFailed);
    const auto failed = FeatureExtractor::extractFromFile("missing.wav");
    Check(Get().calls == std::vector<std::string>{"Open"} &&
          std::all_of(std::begin(failed.features), std::end(failed.features), [](float v) { return v == 0.0f; }),
          "feature: open failure returns zero vector");
    Reset();
    QueueOpen(Status::Ok); QueueRead({Status::EndOfFile, 0});
    const auto empty = FeatureExtractor::extractFromFile("empty.wav");
    Check(Get().calls == std::vector<std::string>{"Open", "Read", "Close"} &&
          std::all_of(std::begin(empty.features), std::end(empty.features), [](float v) { return v == 0.0f; }),
          "feature: immediate EOF returns zero vector");
    Reset();
    QueueOpen(Status::Ok);
    const std::vector<float> zeros(8192);
    for (int i = 0; i < 82; ++i) QueueRead({Status::Ok, 8192}, zeros);
    FeatureExtractor::extractFromFile("long.wav");
    Check(Get().readRequests.size() == 81 &&
          std::all_of(Get().readRequests.begin(), Get().readRequests.end(),
                      [](FrameCount n) { return n == 8192; }),
          "feature: strict cap stops after 81 full chunks");
    return failures ? 1 : 0;
}
