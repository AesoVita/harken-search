#include "FilePlayerDeferredDbWarmupRuntime.h"

#include "platform/filesystem.h"

#include <atomic>
#include <thread>
#include <utility>

namespace aura::file_player {

namespace {

bool IsDatabasePathAvailable(const std::string& dbPathUtf8) {
    if (dbPathUtf8.empty()) return false;
    try {
        return Platform::FileSystem::Exists(dbPathUtf8);
    } catch (...) {
        return false;
    }
}

} // namespace

struct DeferredDatabaseWarmupRuntime::Impl {
    struct Task {
        std::atomic<bool> started{false};
        std::atomic<bool> done{false};
        std::atomic<bool> success{false};
        std::atomic<bool> reported{false};
    };

    explicit Impl(DeferredDatabaseWarmupOperation operationValue)
        : operation(std::move(operationValue)),
          task(std::make_shared<Task>()) {}

    DeferredDatabaseWarmupOperation operation;
    std::shared_ptr<Task> task;
};

DeferredDatabaseWarmupRuntime::DeferredDatabaseWarmupRuntime(
    DeferredDatabaseWarmupOperation operation)
    : impl_(std::make_unique<Impl>(std::move(operation))) {}

DeferredDatabaseWarmupRuntime::~DeferredDatabaseWarmupRuntime() = default;

DeferredDatabaseWarmupStartResult DeferredDatabaseWarmupRuntime::Start(
    std::string dbPathUtf8) {
    const auto task = impl_->task;
    bool expected = false;
    if (!task->started.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return DeferredDatabaseWarmupStartResult::AlreadyStarted;
    }

    if (!IsDatabasePathAvailable(dbPathUtf8)) {
        task->done.store(true, std::memory_order_release);
        return DeferredDatabaseWarmupStartResult::PathUnavailable;
    }

    const auto operation = impl_->operation;
    std::thread([task, operation, dbPathUtf8 = std::move(dbPathUtf8)]() {
        const bool success = operation(dbPathUtf8);
        task->success.store(success, std::memory_order_release);
        task->done.store(true, std::memory_order_release);
    }).detach();
    return DeferredDatabaseWarmupStartResult::Started;
}

bool DeferredDatabaseWarmupRuntime::PollFinished(bool& success) {
    const auto task = impl_->task;
    if (!task->done.load(std::memory_order_acquire)) return false;

    bool expected = false;
    if (!task->reported.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return false;
    }
    success = task->success.load(std::memory_order_acquire);
    return true;
}

} // namespace aura::file_player
