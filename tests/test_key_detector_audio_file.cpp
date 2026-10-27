#include "KeyDetector.h"
#include "audio_file_reader_scripted.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace AudioFileScripted;
using Platform::AudioFile::FrameCount;
using Platform::AudioFile::LegacyWindowsPathPolicy;
using Platform::AudioFile::Status;
namespace {
int failures = 0;
void Check(bool ok, const char* text) { if (!ok) { std::printf("FAIL: %s\n", text); ++failures; } }
void IsZero(const KeyResult& result, const char* text) {
    Check(!result.ok && result.key.empty() && result.camelot.empty() && result.confidence == 0.0f, text);
}
void OpenAndRead(double start, double end, double max, FrameCount expectedSeek,
                 FrameCount expectedRead, Status seekStatus = Status::Ok,
                 Status readStatus = Status::EndOfFile,
                 FrameCount readFrames = 0) {
    Reset(); QueueOpen(Status::Ok);
    if (start > 0.0) QueueSeek(seekStatus);
    QueueRead({readStatus, readFrames}, std::vector<float>(static_cast<std::size_t>(readFrames)));
    const auto result = KeyDetector::detectFile("key.wav", start, end, max);
    const auto& s = Get();
    Check(s.calls == (start > 0.0 ? std::vector<std::string>{"Open", "Seek", "Read", "Close"} :
                                    std::vector<std::string>{"Open", "Read", "Close"}),
          "key: call order");
    Check(s.paths.size() == 1 && s.paths[0] == "key.wav" && s.policies.size() == 1 &&
          s.policies[0] ==
          LegacyWindowsPathPolicy::KeyPermissiveUtf8CanonicalPrefix, "key: path and policy");
    if (start > 0.0) Check(s.seeks.size() == 1 && s.seeks[0] == expectedSeek, "key: seek frame");
    Check(s.readRequests.size() == 1 && s.readRequests[0] == expectedRead, "key: read frame count");
    if (readFrames < 22050) IsZero(result, "key: short positive read remains below threshold");
}
}
int main() {
    OpenAndRead(1.25, 3.75, 1.5, 27562, 33075);
    OpenAndRead(0.0, -1.0, 1.25, 0, 27562);
    OpenAndRead(0.5, 9.0, 2.25, 11025, 49612);
    OpenAndRead(1.0, 2.0, 1.0, 22050, 22050, Status::SeekFailed, Status::EndOfFile, 0);
    OpenAndRead(0.0, -1.0, 1.0, 0, 22050, Status::Ok, Status::DecodeFailed, 100);
    Reset(); QueueOpen(Status::Ok);
    std::vector<float> signal(22050);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = std::sin(2.0 * 3.141592653589793 * 440.0 * i / 22050.0);
    const auto expected = KeyDetector::detect(signal.data(), 22050, 1, 22050);
    Check(expected.ok, "key: deterministic sine direct detection");
    QueueRead({Status::DecodeFailed, 22050}, signal);
    const auto reachesDetect = KeyDetector::detectFile("key.wav", 0.0, -1.0, 1.0);
    Check(Get().calls == std::vector<std::string>{"Open", "Read", "Close"} &&
          Get().readRequests.size() == 1 && Get().readRequests[0] == 22050,
          "key: threshold read reaches detect");
    Check(reachesDetect.ok == expected.ok && reachesDetect.key == expected.key &&
          reachesDetect.camelot == expected.camelot && reachesDetect.isMinor == expected.isMinor &&
          std::fabs(reachesDetect.confidence - expected.confidence) < 0.001f,
          "key: threshold result matches direct detection");
    Reset(); QueueOpen(Status::OpenFailed);
    IsZero(KeyDetector::detectFile("missing.wav", 0.0, -1.0, 1.0), "key: open failure default");
    Check(Get().calls == std::vector<std::string>{"Open"}, "key: open failure has no read/seek");
    return failures ? 1 : 0;
}
