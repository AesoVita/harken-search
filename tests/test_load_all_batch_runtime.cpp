#include "src/LoadAllBatchRuntime.h"

#include <cassert>
#include <vector>

using aura::load_all::LoadAllBatchBeginInput;
using aura::load_all::LoadAllBatchRequest;
using aura::load_all::LoadAllBatchRuntime;
using aura::load_all::LoadAllBatchTerminal;

int main() {
    std::vector<LoadAllBatchRequest> requests;
    std::vector<LoadAllBatchTerminal> terminals;
    LoadAllBatchRuntime runtime;

    runtime.begin(
        LoadAllBatchBeginInput{2, 41, 120},
        [&](const LoadAllBatchRequest& request) { requests.push_back(request); },
        [&](LoadAllBatchTerminal terminal) { terminals.push_back(terminal); });

    assert(runtime.active());
    assert(requests.size() == 1);
    assert(requests[0].viewId == 2);
    assert(requests[0].playlistId == 41);
    assert(requests[0].offset == 0);
    assert(requests[0].limit == 120);
    assert(requests[0].resetResults);

    runtime.onResult(2, 120, true, false);
    assert(runtime.active());
    assert(requests.size() == 2);
    assert(requests[1].offset == 120);
    assert(!requests[1].resetResults);

    runtime.onResult(2, 0, true, true);
    assert(!runtime.active());
    assert(requests.size() == 2);
    assert(terminals.size() == 1);
    assert(terminals[0] == LoadAllBatchTerminal::Finished);

    runtime.begin(
        LoadAllBatchBeginInput{1, -1, 60},
        [&](const LoadAllBatchRequest& request) { requests.push_back(request); },
        [&](LoadAllBatchTerminal terminal) { terminals.push_back(terminal); });
    runtime.onFrame(4);
    assert(!runtime.active());
    assert(terminals.size() == 2);
    assert(terminals[1] == LoadAllBatchTerminal::ViewChanged);

    runtime.begin(
        LoadAllBatchBeginInput{3, 9, 120},
        [&](const LoadAllBatchRequest& request) { requests.push_back(request); },
        [&](LoadAllBatchTerminal terminal) { terminals.push_back(terminal); });
    runtime.reset();
    assert(!runtime.active());
    assert(terminals.size() == 2);

    return 0;
}
