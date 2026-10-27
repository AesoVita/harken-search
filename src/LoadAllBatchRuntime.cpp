#include "LoadAllBatchRuntime.h"

#include <utility>

namespace aura::load_all {

void LoadAllBatchRuntime::begin(LoadAllBatchBeginInput input,
                                SubmitPort submit,
                                TerminalPort terminal) {
    reset();
    viewId_ = input.viewId;
    request_ = {input.viewId, input.playlistId, 0, input.limit, true};
    submit_ = std::move(submit);
    terminal_ = std::move(terminal);
    active_ = true;
    submit_(request_);
}

void LoadAllBatchRuntime::onResult(int currentViewId,
                                   int loadedCount,
                                   bool hasMore,
                                   bool pageEmpty) {
    if (!acceptsResult(currentViewId)) return;
    request_.offset += loadedCount;
    if (!hasMore || pageEmpty) {
        finish(LoadAllBatchTerminal::Finished);
        return;
    }
    request_.resetResults = false;
    submit_(request_);
}

void LoadAllBatchRuntime::onFrame(int currentViewId) {
    if (!active_ || currentViewId == viewId_) return;
    finish(LoadAllBatchTerminal::ViewChanged);
}

void LoadAllBatchRuntime::reset() {
    active_ = false;
    submit_ = {};
    terminal_ = {};
    request_ = {};
    viewId_ = 0;
}

void LoadAllBatchRuntime::finish(LoadAllBatchTerminal terminal) {
    active_ = false;
    submit_ = {};
    auto terminalPort = std::move(terminal_);
    terminal_ = {};
    if (terminalPort) terminalPort(terminal);
}

}  // namespace aura::load_all
