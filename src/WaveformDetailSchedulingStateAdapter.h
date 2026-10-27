#pragma once

#include <string>

struct AppState;

namespace aura::waveform {

void ScheduleDeferredDetail(
    AppState& state,
    const std::string& filePath,
    int targetPeaks);

}  // namespace aura::waveform
