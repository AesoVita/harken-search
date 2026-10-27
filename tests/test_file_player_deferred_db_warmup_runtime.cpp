#include "FilePlayerDeferredDbWarmupRuntime.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

struct BlockingOperation {
    struct State {
        std::mutex mutex;
        std::condition_variable condition;
        std::string receivedPath;
        int callCount = 0;
        bool entered = false;
        bool released = false;
        bool result = false;
        bool returned = false;
    };

    std::shared_ptr<State> state = std::make_shared<State>();

    std::function<bool(const std::string&)> Callback() const {
        const auto shared = state;
        return [shared](const std::string& path) {
            std::unique_lock<std::mutex> lock(shared->mutex);
            shared->receivedPath = path;
            ++shared->callCount;
            shared->entered = true;
            shared->condition.notify_all();
            shared->condition.wait(lock, [&] { return shared->released; });
            const bool result = shared->result;
            shared->returned = true;
            shared->condition.notify_all();
            return result;
        };
    }

    bool WaitUntilEntered() const {
        std::unique_lock<std::mutex> lock(state->mutex);
        return state->condition.wait_for(
            lock, std::chrono::seconds(5), [&] { return state->entered; });
    }

    bool WaitUntilReturned() const {
        std::unique_lock<std::mutex> lock(state->mutex);
        return state->condition.wait_for(
            lock, std::chrono::seconds(5), [&] { return state->returned; });
    }

    void Release(bool result) const {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->result = result;
        state->released = true;
        state->condition.notify_all();
    }
};

bool PollUntilFinished(
    aura::file_player::DeferredDatabaseWarmupRuntime& runtime,
    bool& success) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        if (runtime.PollFinished(success)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

} // namespace

int main() {
    using aura::file_player::DeferredDatabaseWarmupRuntime;
    using aura::file_player::DeferredDatabaseWarmupStartResult;

    const fs::path root = fs::temp_directory_path() /
        ("aris-file-player-warmup-runtime-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const fs::path marker = root / "database-marker.db";
    std::ofstream(marker, std::ios::binary) << "fixture";
    const std::string markerPath = marker.u8string();

    BlockingOperation successfulOperation;
    DeferredDatabaseWarmupRuntime successfulRuntime(
        successfulOperation.Callback());
    if (!Check(
            successfulRuntime.Start(markerPath) ==
                DeferredDatabaseWarmupStartResult::Started,
            "existing path should launch the asynchronous warmup")) return 1;
    if (!Check(successfulOperation.WaitUntilEntered(),
               "injected operation should run on the worker")) return 1;
    {
        std::lock_guard<std::mutex> lock(successfulOperation.state->mutex);
        if (!Check(successfulOperation.state->receivedPath == markerPath,
                   "worker should receive the exact database path")) return 1;
        if (!Check(successfulOperation.state->callCount == 1,
                   "worker should invoke the operation once")) return 1;
    }
    bool success = true;
    if (!Check(!successfulRuntime.PollFinished(success),
               "completion must not publish before the operation returns")) return 1;
    if (!Check(
            successfulRuntime.Start(markerPath) ==
                DeferredDatabaseWarmupStartResult::AlreadyStarted,
            "the warmup session should be one-shot")) return 1;
    successfulOperation.Release(true);
    if (!Check(PollUntilFinished(successfulRuntime, success) && success,
               "successful operation should publish success")) return 1;
    if (!Check(!successfulRuntime.PollFinished(success),
               "completion should be consumed exactly once")) return 1;

    BlockingOperation failedOperation;
    DeferredDatabaseWarmupRuntime failedRuntime(failedOperation.Callback());
    if (!Check(
            failedRuntime.Start(markerPath) ==
                DeferredDatabaseWarmupStartResult::Started,
            "failure case should still launch")) return 1;
    if (!Check(failedOperation.WaitUntilEntered(),
               "failure operation should enter")) return 1;
    failedOperation.Release(false);
    success = true;
    if (!Check(PollUntilFinished(failedRuntime, success) && !success,
               "failed operation should publish failure")) return 1;

    int missingPathCalls = 0;
    DeferredDatabaseWarmupRuntime missingPathRuntime(
        [&](const std::string&) {
            ++missingPathCalls;
            return true;
        });
    const std::string missingPath = (root / "missing.db").u8string();
    if (!Check(
            missingPathRuntime.Start(missingPath) ==
                DeferredDatabaseWarmupStartResult::PathUnavailable,
            "missing path should consume start without launching a worker")) return 1;
    success = true;
    if (!Check(missingPathRuntime.PollFinished(success) && !success,
               "missing path should publish one unsuccessful completion")) return 1;
    if (!Check(missingPathCalls == 0 &&
                   !missingPathRuntime.PollFinished(success),
               "missing path must not call the operation or republish")) return 1;

    BlockingOperation destructionOperation;
    {
        DeferredDatabaseWarmupRuntime runtime(destructionOperation.Callback());
        if (!Check(
                runtime.Start(markerPath) ==
                    DeferredDatabaseWarmupStartResult::Started,
                "destruction case should launch")) return 1;
        if (!Check(destructionOperation.WaitUntilEntered(),
                   "destruction worker should enter")) return 1;
    }
    destructionOperation.Release(true);
    if (!Check(destructionOperation.WaitUntilReturned(),
               "shared worker state should survive runtime destruction")) return 1;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    fs::remove_all(root);
    std::cout << "PASS: file player deferred database warmup runtime contracts\n";
    return 0;
}
