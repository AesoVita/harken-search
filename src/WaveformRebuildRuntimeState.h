#pragma once

struct AppState;

namespace aura::waveform_rebuild {

class WaveformRebuildRuntimeState {
public:
    void Reset(AppState& state, bool cancelWorkerQueue);

    bool batchMiniOnly() const noexcept;
    void setBatchMiniOnly(bool value) noexcept;

    bool incrementalMiniOnly() const noexcept;
    void setIncrementalMiniOnly(bool value) noexcept;

private:
    bool batchMiniOnly_ = false;
    bool incrementalMiniOnly_ = false;
};

}  // namespace aura::waveform_rebuild
