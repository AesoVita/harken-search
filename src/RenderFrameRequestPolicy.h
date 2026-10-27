#pragma once

// @stage7-contract
// API contract: consumes deferred render-frame request flags synchronously on
// the caller thread and returns a value snapshot. The function owns no input,
// performs no I/O, allocates no memory, and preserves inactive flag values.
enum class DeferredWaveformRebuild {
    None,
    Incremental,
    Full,
};

struct DeferredRenderFrameRequests {
    int rescanRow = -1;
    bool scanBpm = false;
    DeferredWaveformRebuild waveformRebuild = DeferredWaveformRebuild::None;
};

DeferredRenderFrameRequests ConsumeDeferredRenderFrameRequests(
    int& rescanRow,
    bool& scanBpm,
    bool& waveformRebuild,
    bool& waveformRebuildFull) noexcept;
