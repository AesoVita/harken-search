#pragma once

#include "AppState.h"

#include <string>
#include <vector>

namespace aura::audio_export {

struct ChannelExportCompletion {
    std::string channelLabel;
    AppState::ChannelExportJobResult result;
};

// Non-blocking: consumes only ready jobs, normalizes future exceptions, and
// removes completed jobs. Presentation remains owned by the application root.
std::vector<ChannelExportCompletion> PollChannelExportCompletions(
    std::vector<AppState::ChannelExportJob>& jobs);

}  // namespace aura::audio_export
