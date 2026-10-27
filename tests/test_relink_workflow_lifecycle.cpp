// Native lifecycle coverage for the RelinkWorkflow thread-ownership pattern.
//
// The RelinkWorkflow functions (EvaluateRelinkForRecord, PumpRelinkWorkflow,
// ShutdownRelinkWorkflow) live inside RelinkWorkflow.cpp and depend on the
// full AppState + Database + ImGui dependency graph.  Rather than linking the
// entire executable, this test mirrors the lifecycle-relevant subset of
// AppState, exercises the thread-retirement / generation-invalidation / mutex
// protocol locally, and then verifies that a real AppState (compiled as part
// of the Aris target) passes the same static-contract assertions via the
// existing Python boundary test.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
}

// ---------------------------------------------------------------------------
// Minimal mirror of the thread-lifetime fields from AppState that
// RelinkWorkflow actually touches.  This avoids pulling in AudioEngine,
// Database, ImGui, etc.
// ---------------------------------------------------------------------------

struct RelinkLifecycleState {
    // Banner search
    std::atomic<bool>        bannerSearchDone{false};
    std::atomic<uint64_t>    bannerSearchGeneration{0};
    std::thread              bannerSearchThread;

    // Tool search
    std::atomic<bool>        toolSearchDone{false};
    std::atomic<uint64_t>    toolSearchGeneration{0};
    std::thread              toolSearchThread;

    // Shared
    std::mutex               resultMutex;
    std::vector<std::thread> retiredThreads;

    // Pending publication state
    std::vector<std::string> pendingCandidates;
    uint64_t                 pendingBannerGeneration = 0;
    bool                     bannerSearching = false;
};

// Retire a joinable worker: move it into retired so Shutdown can join later.
static void RetireThread(RelinkLifecycleState& s, std::thread& worker) {
    if (worker.joinable()) {
        s.retiredThreads.emplace_back(std::move(worker));
    }
}

// Shutdown protocol mirroring aura::ui::ShutdownRelinkWorkflow.
static void ShutdownLifecycle(RelinkLifecycleState& s) {
    // Invalidate generations so in-flight workers discard results.
    s.bannerSearchGeneration.fetch_add(1, std::memory_order_acq_rel);
    s.toolSearchGeneration.fetch_add(1, std::memory_order_acq_rel);
    s.bannerSearchDone.store(false, std::memory_order_release);
    s.toolSearchDone.store(false, std::memory_order_release);

    RetireThread(s, s.bannerSearchThread);
    RetireThread(s, s.toolSearchThread);
    for (auto& worker : s.retiredThreads) {
        if (worker.joinable()) worker.join();
    }
    s.retiredThreads.clear();

    {
        std::lock_guard<std::mutex> lock(s.resultMutex);
        s.pendingCandidates.clear();
        s.pendingBannerGeneration = 0;
    }
    s.bannerSearching = false;
}

// Pump protocol: only publish if the pending generation matches current.
static void PumpBanner(RelinkLifecycleState& s,
                       std::vector<std::string>& target) {
    if (!s.bannerSearchDone.exchange(false)) return;

    std::vector<std::string> candidates;
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s.resultMutex);
        generation = s.pendingBannerGeneration;
        candidates = std::move(s.pendingCandidates);
        s.pendingBannerGeneration = 0;
    }
    if (generation == s.bannerSearchGeneration.load(std::memory_order_acquire)) {
        s.bannerSearching = false;
        target = std::move(candidates);
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void TestShutdownJoinsFinishedWorker() {
    RelinkLifecycleState s;
    std::promise<void> started;
    std::atomic<bool> finished{false};

    s.bannerSearchThread = std::thread([&]() {
        started.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        finished.store(true);
    });

    started.get_future().wait();
    Check(s.bannerSearchThread.joinable(), "worker joinable after start");

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Check(finished.load(), "worker finished");

    ShutdownLifecycle(s);

    Check(!s.bannerSearchThread.joinable(), "worker joined after shutdown");
    Check(s.retiredThreads.empty(), "retired threads cleared");
    Check(!s.bannerSearching, "searching cleared");
    Check(s.pendingCandidates.empty(), "pending cleared");
    Check(s.pendingBannerGeneration == 0, "generation reset");
}

void TestShutdownClearsBothWorkers() {
    RelinkLifecycleState s;
    std::atomic<int> bannerDone{0};
    std::atomic<int> toolDone{0};

    s.bannerSearchThread = std::thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        bannerDone.store(1);
    });
    s.toolSearchThread = std::thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        toolDone.store(1);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Check(bannerDone.load() == 1, "banner worker done");
    Check(toolDone.load() == 1, "tool worker done");

    ShutdownLifecycle(s);

    Check(!s.bannerSearchThread.joinable(), "banner joined");
    Check(!s.toolSearchThread.joinable(), "tool joined");
    Check(s.retiredThreads.empty(), "retired cleared");
    Check(!s.bannerSearching, "searching off");
}

void TestGenerationPreventsStaleResults() {
    RelinkLifecycleState s;

    // Shutdown bumps generation.
    ShutdownLifecycle(s);
    const uint64_t postShutdown = s.bannerSearchGeneration.load();

    // Simulate old worker publishing stale generation.
    {
        std::lock_guard<std::mutex> lock(s.resultMutex);
        s.pendingCandidates = {"stale/path.wav"};
        s.pendingBannerGeneration = 0;  // old
    }
    s.bannerSearchDone.store(true, std::memory_order_release);

    std::vector<std::string> results = {"pre-existing"};
    PumpBanner(s, results);

    // Pump must reject because generation doesn't match.
    Check(results == std::vector<std::string>{"pre-existing"},
          "pump rejects stale generation");
    Check(s.pendingBannerGeneration == 0, "pending gen cleared after pump");
}

void TestShutdownIsIdempotent() {
    RelinkLifecycleState s;

    ShutdownLifecycle(s);
    ShutdownLifecycle(s);

    Check(!s.bannerSearching, "searching still false after double shutdown");
    Check(s.retiredThreads.empty(), "no retired threads leaked");
    Check(s.pendingCandidates.empty(), "candidates empty");
    Check(s.pendingBannerGeneration == 0, "generation zero");
}

}  // namespace

int main() {
    TestShutdownJoinsFinishedWorker();
    TestShutdownClearsBothWorkers();
    TestGenerationPreventsStaleResults();
    TestShutdownIsIdempotent();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "All RelinkWorkflow lifecycle tests passed.\n");
    return 0;
}