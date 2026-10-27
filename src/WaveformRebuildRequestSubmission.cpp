#include "WaveformRebuildRequestSubmission.h"

namespace aura::waveform_rebuild {

WaveformWorkerRebuildSubmissionPort::WaveformWorkerRebuildSubmissionPort(
    WaveformWorker& worker) noexcept
    : worker_(worker) {}

bool WaveformWorkerRebuildSubmissionPort::submitMini(
    const std::string& filePath) {
    return worker_.submitVisibleRequest(filePath);
}

bool WaveformWorkerRebuildSubmissionPort::submitDetail(
    const std::string& filePath,
    int64_t fileId) {
    return worker_.submitDetailRequest(filePath, fileId);
}

WaveformRebuildSubmissionResult SubmitWaveformRebuildRequest(
    WaveformRebuildSubmissionPort& port,
    const WaveformRebuildSubmissionRequest& request) {
    const bool submitted = request.miniOnly
        ? port.submitMini(request.filePath)
        : port.submitDetail(request.filePath, request.fileId);
    return submitted
        ? WaveformRebuildSubmissionResult::Submitted
        : WaveformRebuildSubmissionResult::Rejected;
}

}  // namespace aura::waveform_rebuild
