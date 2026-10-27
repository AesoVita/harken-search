#pragma once

#include "Database.h"

namespace aura::waveform_rebuild {

enum class AdaptiveMiniFormatFinalizationReason {
    NoWork,
    RebuildCompleted,
    Cancelled,
};

struct AdaptiveMiniFormatFinalizationState {
    bool rebuildPending = false;
    bool stampPending = false;
};

struct AdaptiveMiniFormatFinalizationResult {
    AdaptiveMiniFormatFinalizationState state;
    bool stampAttempted = false;
    bool stampSucceeded = false;
};

class AdaptiveMiniFormatStampPort {
public:
    virtual ~AdaptiveMiniFormatStampPort() = default;

    virtual bool markUpgradeComplete() = 0;
};

class DatabaseAdaptiveMiniFormatStampPort final
    : public AdaptiveMiniFormatStampPort {
public:
    explicit DatabaseAdaptiveMiniFormatStampPort(Database& database) noexcept;

    bool markUpgradeComplete() override;

private:
    Database& database_;
};

AdaptiveMiniFormatFinalizationResult FinalizeAdaptiveMiniWaveformFormat(
    AdaptiveMiniFormatStampPort& port,
    AdaptiveMiniFormatFinalizationReason reason,
    AdaptiveMiniFormatFinalizationState state);

}  // namespace aura::waveform_rebuild
