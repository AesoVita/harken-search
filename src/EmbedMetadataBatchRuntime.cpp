#include "EmbedMetadataBatchRuntime.h"

#include <chrono>

namespace aura::metadata {

void EmbedMetadataBatchRuntime::pump(EmbedMetadataBatchStatePort& state,
                                     EmbedMetadataTaskPort& tasks) {
    if (!state.active()) return;

    if (!inFlight_ && state.progress() < state.total()) {
        inFlight_ = true;
        future_ = tasks.launch(state.currentTask());
    }

    if (inFlight_ && future_.valid() &&
        future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        future_.get();
        state.advance();
        inFlight_ = false;
    }

    if (state.progress() >= state.total() && !inFlight_) {
        state.finish();
    }
}

}  // namespace aura::metadata
