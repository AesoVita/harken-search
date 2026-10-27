#pragma once

struct AppState;

namespace aura::search {

void StartDeferredCoverPriming();
void PumpDeferredCoverPriming(AppState& state, int maxPerFrame = 20);

}  // namespace aura::search
