#pragma once

#include "RandomShufflePlaybackWorkflow.h"

#include <functional>

struct AppState;
struct AsyncSearchResult;

namespace aura::search {

class AsyncSearchRandomShufflePlaybackStateAdapter final {
public:
    using ScheduleWaveformDetail =
        std::function<void(AppState&, const std::string&, int)>;
    using ResetWaveformZoom = std::function<void(AppState&)>;
    using MarkFilePlayed =
        std::function<void(AppState&, const std::string&, std::int64_t)>;

    AsyncSearchRandomShufflePlaybackStateAdapter(
        AppState& state,
        AsyncSearchResult& result,
        ScheduleWaveformDetail scheduleWaveformDetail,
        ResetWaveformZoom resetWaveformZoom,
        MarkFilePlayed markFilePlayed);

    void apply();

private:
    AppState& state_;
    AsyncSearchResult& result_;
    ScheduleWaveformDetail scheduleWaveformDetail_;
    ResetWaveformZoom resetWaveformZoom_;
    MarkFilePlayed markFilePlayed_;
};

}  // namespace aura::search
