#include "AsyncSnapshotWorker.h"

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
    } else {
        std::cerr << "FAIL: " << message << '\n';
        ++g_failures;
    }
}

template <typename Worker>
auto WaitForResult(Worker& worker) {
    using namespace std::chrono_literals;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        if (auto result = worker.Poll()) return result;
        std::this_thread::sleep_for(1ms);
    }
    return decltype(worker.Poll()){};
}

void TestSubmitDoesNotWaitForProbe() {
    std::promise<void> enteredPromise;
    auto entered = enteredPromise.get_future();
    std::promise<void> releasePromise;
    auto release = releasePromise.get_future().share();

    HarkenSearch::AsyncSnapshotWorker<int, int> worker(
        [&enteredPromise, release](int value) mutable {
            enteredPromise.set_value();
            release.wait();
            return value + 1;
        });

    Check(worker.Submit(41),
          "BDD: a DAW probe snapshot is accepted without running the probe inline");
    Check(entered.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
          "BDD: the persistent worker receives the probe snapshot");
    Check(!worker.Poll().has_value(),
          "BDD: the render thread polls without blocking while the probe is still running");
    Check(!worker.Submit(99),
          "BDD: a second periodic probe cannot overlap the in-flight snapshot");

    releasePromise.set_value();
    const auto result = WaitForResult(worker);
    Check(result.has_value() && *result == 42,
          "BDD: the completed probe result is harvested on a later render tick");
}

void TestRequestUsesImmutableSnapshot() {
    std::promise<void> releasePromise;
    auto release = releasePromise.get_future().share();
    HarkenSearch::AsyncSnapshotWorker<std::vector<int>, int> worker(
        [release](std::vector<int> snapshot) {
            release.wait();
            return snapshot.front();
        });

    std::vector<int> liveConfiguration{7};
    Check(worker.Submit(liveConfiguration),
          "BDD: the live DAW configuration is copied into a worker-owned snapshot");
    liveConfiguration.front() = 88;
    releasePromise.set_value();

    const auto result = WaitForResult(worker);
    Check(result.has_value() && *result == 7,
          "BDD: edits made while probing cannot race with or alter the worker snapshot");
}

} // namespace

int main() {
    TestSubmitDoesNotWaitForProbe();
    TestRequestUsesImmutableSnapshot();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
