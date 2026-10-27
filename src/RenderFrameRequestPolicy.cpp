#include "RenderFrameRequestPolicy.h"


DeferredRenderFrameRequests ConsumeDeferredRenderFrameRequests(int& rescanRow, bool& scanBpm, bool& waveformRebuild, bool& waveformRebuildFull) noexcept {
    DeferredRenderFrameRequests requests;
    if (rescanRow >= 0) {
        requests.rescanRow = rescanRow;
        rescanRow = -1;
    }
    if (scanBpm) {
        requests.scanBpm = true;
        scanBpm = false;
    }
    if (waveformRebuild) {
        requests.waveformRebuild = waveformRebuildFull
            ? DeferredWaveformRebuild::Full
            : DeferredWaveformRebuild::Incremental;
        waveformRebuild = false;
        waveformRebuildFull = false;
    }
    return requests;
}
