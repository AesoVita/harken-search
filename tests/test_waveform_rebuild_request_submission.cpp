#include "src/WaveformRebuildRequestSubmission.h"

#include <cstdio>
#include <string>
#include <vector>

bool WaveformWorker::submitVisibleRequest(const std::string&) {
    return true;
}

bool WaveformWorker::submitDetailRequest(const std::string&, int64_t) {
    return true;
}

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::printf("PASS: %s\n", message);
    } else {
        std::printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

class FakePort final
    : public aura::waveform_rebuild::WaveformRebuildSubmissionPort {
public:
    bool returnValue = false;
    std::vector<std::string> calls;

    bool submitMini(const std::string& filePath) override {
        calls.push_back("mini:" + filePath);
        return returnValue;
    }

    bool submitDetail(const std::string& filePath, int64_t fileId) override {
        calls.push_back(
            "detail:" + filePath + ":" + std::to_string(fileId));
        return returnValue;
    }
};

}  // namespace

int main() {
    using aura::waveform_rebuild::SubmitWaveformRebuildRequest;
    using aura::waveform_rebuild::WaveformRebuildSubmissionRequest;
    using aura::waveform_rebuild::WaveformRebuildSubmissionResult;

    const std::string miniPath = "mini.wav";
    FakePort mini;
    mini.returnValue = true;
    Check(SubmitWaveformRebuildRequest(
              mini, WaveformRebuildSubmissionRequest{miniPath, 17, true}) ==
              WaveformRebuildSubmissionResult::Submitted,
          "accepted mini request reports submitted");
    Check(mini.calls == std::vector<std::string>{"mini:mini.wav"},
          "mini request uses only the path-only mini route");

    const std::string detailPath = "detail.wav";
    FakePort detail;
    detail.returnValue = true;
    Check(SubmitWaveformRebuildRequest(
              detail, WaveformRebuildSubmissionRequest{detailPath, 42, false}) ==
              WaveformRebuildSubmissionResult::Submitted,
          "accepted detail request reports submitted");
    Check(detail.calls == std::vector<std::string>{"detail:detail.wav:42"},
          "detail request preserves path and file ID");

    const std::string rejectedPath = "rejected.wav";
    FakePort rejected;
    Check(SubmitWaveformRebuildRequest(
              rejected,
              WaveformRebuildSubmissionRequest{rejectedPath, 99, false}) ==
              WaveformRebuildSubmissionResult::Rejected,
          "worker rejection reports rejected");
    Check(rejected.calls ==
              std::vector<std::string>{"detail:rejected.wav:99"},
          "rejected request still calls exactly one selected route");

    std::printf("\n=== Waveform Rebuild Request Submission: %d failure(s) ===\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
