#pragma once

#include "Database.h"

namespace aura::waveform_rebuild {

struct WaveformRebuildFailureRequest {
    const AudioRecord& record;
    bool fullMode = false;
    bool previouslyFailed = false;
    bool incrementalMiniOnly = false;
};

class WaveformRebuildFailureSink {
public:
    virtual ~WaveformRebuildFailureSink() = default;

    virtual void persistPreviousFailure(
        const AudioRecord& record,
        const char* reason) = 0;
};

class DatabaseWaveformRebuildFailureSink final
    : public WaveformRebuildFailureSink {
public:
    explicit DatabaseWaveformRebuildFailureSink(Database& database) noexcept;

    void persistPreviousFailure(
        const AudioRecord& record,
        const char* reason) override;

private:
    Database& database_;
};

bool PersistPreviousWaveformRebuildFailure(
    WaveformRebuildFailureSink& sink,
    const WaveformRebuildFailureRequest& request);

}  // namespace aura::waveform_rebuild
