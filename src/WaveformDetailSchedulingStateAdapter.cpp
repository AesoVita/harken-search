#include "WaveformDetailSchedulingStateAdapter.h"

#include "AppState.h"
#include "WaveformPresentationPolicy.h"
#include "imgui.h"

namespace aura::waveform {

void ScheduleDeferredDetail(
    AppState& state,
    const std::string& filePath,
    int targetPeaks) {
    state.pendingDetailFilePath = filePath;
    state.pendingDetailTime = ImGui::GetTime();
    state.pendingDetailPeaks = NormalizeMainWaveformRequestPeaks(
        targetPeaks,
        AppState::WAVEFORM_DEFAULT_DETAIL_PEAKS);

    if (!filePath.empty() && !state.waveformRebuildActive) {
        state.waveformWorker.cancelAllPending();
        state.meterFollowupPending.clear();
        state.waveformFailed.erase(filePath);
        state.waveformMiniCacheMiss.erase(filePath);
        state.visibleFilePaths.insert(filePath);
        state.addPendingWaveform(filePath, true);
    }
}

}  // namespace aura::waveform

void ScheduleDeferredWaveformDetail(AppState& state,
    const std::string& filePath,
    int targetPeaks) {
    aura::waveform::ScheduleDeferredDetail(
        state, filePath, targetPeaks);
}
