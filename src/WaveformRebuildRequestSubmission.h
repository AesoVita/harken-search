#pragma once

#include "AsyncWorker.h"

#include <cstdint>
#include <string>

namespace aura::waveform_rebuild {

enum class WaveformRebuildSubmissionResult {
    Submitted,
    Rejected,
};

struct WaveformRebuildSubmissionRequest {
    const std::string& filePath;
    int64_t fileId = 0;
    bool miniOnly = false;
};

class WaveformRebuildSubmissionPort {
public:
    virtual ~WaveformRebuildSubmissionPort() = default;

    virtual bool submitMini(const std::string& filePath) = 0;
    virtual bool submitDetail(
        const std::string& filePath,
        int64_t fileId) = 0;
};

class WaveformWorkerRebuildSubmissionPort final
    : public WaveformRebuildSubmissionPort {
public:
    explicit WaveformWorkerRebuildSubmissionPort(
        WaveformWorker& worker) noexcept;

    bool submitMini(const std::string& filePath) override;
    bool submitDetail(
        const std::string& filePath,
        int64_t fileId) override;

private:
    WaveformWorker& worker_;
};

WaveformRebuildSubmissionResult SubmitWaveformRebuildRequest(
    WaveformRebuildSubmissionPort& port,
    const WaveformRebuildSubmissionRequest& request);

}  // namespace aura::waveform_rebuild
