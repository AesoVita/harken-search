#include "src/EmbedMetadataBatchRuntime.h"

#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace aura::metadata;

namespace {
int failures = 0;

void Check(bool value, const char* message) {
    if (value) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

struct FakeState final : EmbedMetadataBatchStatePort {
    bool activeValue = false;
    int progressValue = 0;
    int totalValue = 0;
    std::vector<EmbedMetadataTask> tasks;
    int advances = 0;
    int finishes = 0;

    FakeState() = default;
    FakeState(bool active, int progress, int total,
              std::vector<EmbedMetadataTask> initialTasks)
        : activeValue(active),
          progressValue(progress),
          totalValue(total),
          tasks(std::move(initialTasks)) {}

    bool active() const override { return activeValue; }
    int progress() const override { return progressValue; }
    int total() const override { return totalValue; }
    EmbedMetadataTask currentTask() const override {
        return tasks.at(static_cast<std::size_t>(progressValue));
    }
    void advance() override {
        ++progressValue;
        ++advances;
    }
    void finish() override {
        activeValue = false;
        tasks.clear();
        ++finishes;
    }
};

struct PendingLaunch {
    std::promise<bool> promise;
};

struct FakeTasks final : EmbedMetadataTaskPort {
    std::vector<EmbedMetadataTask> launched;
    std::vector<PendingLaunch> pending;

    std::future<bool> launch(EmbedMetadataTask task) override {
        launched.push_back(std::move(task));
        pending.emplace_back();
        return pending.back().promise.get_future();
    }
};

EmbedMetadataTask Task(std::string path) {
    return {std::move(path), "cover-key", "folder-cover.png"};
}
}  // namespace

int main() {
    {
        EmbedMetadataBatchRuntime runtime;
        FakeState state;
        FakeTasks tasks;
        runtime.pump(state, tasks);
        Check(tasks.launched.empty(), "inactive batch does not launch");
        Check(!runtime.inFlight() && state.advances == 0 && state.finishes == 0,
              "inactive batch has no effects");
    }
    {
        EmbedMetadataBatchRuntime runtime;
        FakeState state{true, 0, 1, {Task("one.wav")}};
        FakeTasks tasks;
        runtime.pump(state, tasks);
        Check(tasks.launched.size() == 1 && tasks.launched[0].path == "one.wav",
              "first pump launches the current owned task");
        Check(runtime.inFlight(), "pending launch is retained");
        runtime.pump(state, tasks);
        Check(tasks.launched.size() == 1 && state.advances == 0,
              "not-ready future neither relaunches nor advances");
        tasks.pending[0].promise.set_value(true);
        runtime.pump(state, tasks);
        Check(state.advances == 1, "ready true result advances exactly once");
        Check(state.finishes == 1 && !state.activeValue && state.tasks.empty(),
              "final ready result finishes and clears in the same pump");
        Check(!runtime.inFlight(), "final ready result clears in-flight state");
    }
    {
        EmbedMetadataBatchRuntime runtime;
        FakeState state{true, 0, 1, {Task("false.wav")}};
        FakeTasks tasks;
        runtime.pump(state, tasks);
        tasks.pending[0].promise.set_value(false);
        runtime.pump(state, tasks);
        Check(state.advances == 1 && state.finishes == 1,
              "false task result is ignored and still completes the record");
    }
    {
        EmbedMetadataBatchRuntime runtime;
        FakeState state{true, 0, 2, {Task("a.wav"), Task("b.wav")}};
        FakeTasks tasks;
        runtime.pump(state, tasks);
        tasks.pending[0].promise.set_value(true);
        runtime.pump(state, tasks);
        Check(state.progressValue == 1 && state.finishes == 0,
              "non-final ready result advances without finishing");
        runtime.pump(state, tasks);
        Check(tasks.launched.size() == 2 && tasks.launched[1].path == "b.wav",
              "next pump launches the next record");
        tasks.pending[1].promise.set_value(true);
        runtime.pump(state, tasks);
        Check(state.progressValue == 2 && state.finishes == 1,
              "multi-record sequence finishes after the final record");
    }
    {
        EmbedMetadataBatchRuntime runtime;
        FakeState state{true, 0, 1, {Task("preserve.wav")}};
        FakeTasks tasks;
        runtime.pump(state, tasks);
        state.activeValue = false;
        tasks.pending[0].promise.set_value(true);
        runtime.pump(state, tasks);
        Check(runtime.inFlight() && state.advances == 0 && state.finishes == 0,
              "inactive batch preserves an existing ready in-flight task");
        state.activeValue = true;
        runtime.pump(state, tasks);
        Check(state.advances == 1 && state.finishes == 1,
              "reactivation consumes the preserved future once");
    }


    {
        struct ReadyRejectTasks final : EmbedMetadataTaskPort {
            std::atomic<int> asyncBodies{0};
            std::atomic<int> readyRejects{0};
            std::future<bool> launch(EmbedMetadataTask task) override {
                if (task.path.rfind("\\\\", 0) == 0) {
                    ++readyRejects;
                    std::promise<bool> ready;
                    ready.set_value(false);
                    return ready.get_future();
                }
                ++asyncBodies;
                return std::async(std::launch::async, [] { return true; });
            }
        };
        EmbedMetadataBatchRuntime runtime;
        FakeState state{true, 0, 1, {Task("\\\\nas\\\\share\\\\a.wav")}};
        ReadyRejectTasks tasks;
        runtime.pump(state, tasks);
        Check(tasks.readyRejects == 1, "network launch rejects on caller thread");
        Check(tasks.asyncBodies == 0, "network launch does not start async body");
        Check(state.finishes == 1 && !state.activeValue,
              "ready false still finishes batch");
        Check(!runtime.inFlight(), "ready reject clears in-flight");
    }
    {
        struct Heap {
            std::promise<void> startGate;
            std::promise<void> bodyDone;
            std::atomic<int> bodies{0};
            std::atomic<bool> portsAlive{true};
        };
        auto heap = std::make_shared<Heap>();
        EmbedMetadataBatchRuntime runtime;
        struct CaptureTasks final : EmbedMetadataTaskPort {
            std::shared_ptr<Heap> heap;
            std::future<bool> launch(EmbedMetadataTask task) override {
                const std::string path = task.path;
                auto h = heap;
                return std::async(std::launch::async, [h, path]() -> bool {
                    h->startGate.get_future().wait();
                    if (path.empty()) return false;
                    h->bodies.fetch_add(1);
                    h->bodyDone.set_value();
                    return true;
                });
            }
        };
        auto state = std::make_unique<FakeState>(
            true, 0, 1, std::vector<EmbedMetadataTask>{Task("local.wav")});
        auto tasks = std::make_unique<CaptureTasks>();
        tasks->heap = heap;
        runtime.pump(*state, *tasks);
        Check(runtime.inFlight(), "non-network launch stays in flight");
        state.reset();
        tasks.reset();
        heap->portsAlive.store(false);
        heap->startGate.set_value();
        heap->bodyDone.get_future().wait();
        FakeState state2{true, 0, 1, {Task("local.wav")}};
        CaptureTasks tasks2;
        tasks2.heap = heap;
        runtime.pump(state2, tasks2);
        Check(heap->bodies.load() == 1, "captured values ran after ports died");
        Check(!heap->portsAlive.load(), "completion did not require live launch ports");
        Check(state2.finishes == 1, "late ready result finishes replacement ports");
        Check(!runtime.inFlight(), "late ready result clears in-flight");
    }

    if (failures) return 1;
    std::cout << "PASS: embed metadata batch runtime\n";
    return 0;
}
