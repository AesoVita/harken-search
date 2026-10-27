#include "DatabaseSwitchWorkerRuntime.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

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
        int callCount = 0;
        bool entered = false;
        bool released = false;
        bool result = false;
        bool returned = false;
    };

    std::shared_ptr<State> state = std::make_shared<State>();

    aura::database_switch::DatabaseSwitchOperation Callback() const {
        const auto shared = state;
        return [shared]() {
            std::unique_lock<std::mutex> lock(shared->mutex);
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
    aura::database_switch::DatabaseSwitchWorkerRuntime& runtime,
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
    using aura::database_switch::DatabaseSwitchWorkerRuntime;

    DatabaseSwitchWorkerRuntime runtime;
    BlockingOperation successfulOperation;
    runtime.Start(successfulOperation.Callback());
    if (!Check(successfulOperation.WaitUntilEntered(),
               "successful operation should enter on the worker")) return 1;
    {
        std::lock_guard<std::mutex> lock(successfulOperation.state->mutex);
        if (!Check(successfulOperation.state->callCount == 1,
                   "successful operation should run exactly once")) return 1;
    }
    bool success = false;
    if (!Check(!runtime.PollFinished(success) && !success,
               "poll before release should preserve the output sentinel")) return 1;
    successfulOperation.Release(true);
    if (!Check(PollUntilFinished(runtime, success) && success,
               "successful operation should publish success")) return 1;
    if (!Check(!runtime.PollFinished(success),
               "successful completion should be consumed exactly once")) return 1;

    BlockingOperation failedOperation;
    runtime.Start(failedOperation.Callback());
    if (!Check(failedOperation.WaitUntilEntered(),
               "reused runtime should launch a second operation")) return 1;
    failedOperation.Release(false);
    success = true;
    if (!Check(PollUntilFinished(runtime, success) && !success,
               "second cycle should publish failure independently")) return 1;
    {
        std::lock_guard<std::mutex> lock(failedOperation.state->mutex);
        if (!Check(failedOperation.state->callCount == 1,
                   "second operation should run exactly once")) return 1;
    }

    DatabaseSwitchWorkerRuntime waitedRuntime;
    std::atomic<int> waitedCalls{0};
    waitedRuntime.Start([&]() {
        waitedCalls.fetch_add(1, std::memory_order_relaxed);
        return true;
    });
    waitedRuntime.Wait();
    success = false;
    if (!Check(waitedCalls.load(std::memory_order_relaxed) == 1 &&
                   waitedRuntime.PollFinished(success) && success,
               "explicit wait should join without consuming the result")) return 1;
    if (!Check(!waitedRuntime.PollFinished(success),
               "waited completion should still be consumed once")) return 1;

    BlockingOperation destructionOperation;
    std::thread delayedRelease;
    {
        DatabaseSwitchWorkerRuntime destructionRuntime;
        destructionRuntime.Start(destructionOperation.Callback());
        if (!Check(destructionOperation.WaitUntilEntered(),
                   "destruction operation should enter")) return 1;
        delayedRelease = std::thread([destructionOperation]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            destructionOperation.Release(true);
        });
    }
    delayedRelease.join();
    if (!Check(destructionOperation.WaitUntilReturned(),
               "runtime destruction should wait for outstanding work")) return 1;

    std::cout << "PASS: database switch worker runtime contracts\n";
    return 0;
}
