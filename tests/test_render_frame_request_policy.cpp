#include "RenderFrameRequestPolicy.h"

#include <cassert>


static void TestEmptyRequests() {
    int rescanRow = -1;
    bool scanBpm = false;
    bool rebuild = false;
    bool full = false;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.rescanRow == -1);
    assert(!requests.scanBpm);
    assert(requests.waveformRebuild == DeferredWaveformRebuild::None);
    assert(rescanRow == -1);
    assert(!scanBpm);
    assert(!rebuild);
    assert(!full);
}


static void TestCombinedFullRequests() {
    int rescanRow = 7;
    bool scanBpm = true;
    bool rebuild = true;
    bool full = true;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.rescanRow == 7);
    assert(requests.scanBpm);
    assert(requests.waveformRebuild == DeferredWaveformRebuild::Full);
    assert(rescanRow == -1);
    assert(!scanBpm);
    assert(!rebuild);
    assert(!full);
}


static void TestIncrementalRebuild() {
    int rescanRow = -1;
    bool scanBpm = false;
    bool rebuild = true;
    bool full = false;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.waveformRebuild == DeferredWaveformRebuild::Incremental);
    assert(!rebuild);
    assert(!full);
}


static void TestFullRebuild() {
    int rescanRow = -1;
    bool scanBpm = false;
    bool rebuild = true;
    bool full = true;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.waveformRebuild == DeferredWaveformRebuild::Full);
    assert(!rebuild);
    assert(!full);
}


static void TestStaleFullFlagWithoutRequestIsPreserved() {
    int rescanRow = -1;
    bool scanBpm = false;
    bool rebuild = false;
    bool full = true;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.waveformRebuild == DeferredWaveformRebuild::None);
    assert(!rebuild);
    assert(full);
}


static void TestNegativeRescanSentinelIsPreserved() {
    int rescanRow = -2;
    bool scanBpm = false;
    bool rebuild = false;
    bool full = false;

    const auto requests = ConsumeDeferredRenderFrameRequests(
        rescanRow, scanBpm, rebuild, full);

    assert(requests.rescanRow == -1);
    assert(rescanRow == -2);
}


int main() {
    TestEmptyRequests();
    TestCombinedFullRequests();
    TestIncrementalRebuild();
    TestFullRebuild();
    TestStaleFullFlagWithoutRequestIsPreserved();
    TestNegativeRescanSentinelIsPreserved();
    return 0;
}
