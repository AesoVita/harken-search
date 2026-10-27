#include "DatabaseSwitchWorkerRuntime.h"

#include <atomic>
#include <thread>
#include <utility>

namespace aura::database_switch {

struct DatabaseSwitchWorkerRuntime::Impl {
    std::thread worker;
    std::atomic<bool> done{false};
    std::atomic<bool> success{false};
};

DatabaseSwitchWorkerRuntime::DatabaseSwitchWorkerRuntime()
    : impl_(std::make_unique<Impl>()) {}

DatabaseSwitchWorkerRuntime::~DatabaseSwitchWorkerRuntime() {
    Wait();
}

void DatabaseSwitchWorkerRuntime::Start(DatabaseSwitchOperation operation) {
    Wait();
    impl_->success.store(false, std::memory_order_release);
    impl_->done.store(false, std::memory_order_release);
    impl_->worker = std::thread(
        [impl = impl_.get(), operation = std::move(operation)]() mutable {
            const bool result = operation();
            impl->success.store(result, std::memory_order_release);
            impl->done.store(true, std::memory_order_release);
        });
}

bool DatabaseSwitchWorkerRuntime::PollFinished(bool& success) {
    if (!impl_->done.load(std::memory_order_acquire)) return false;
    Wait();
    success = impl_->success.load(std::memory_order_acquire);
    impl_->done.store(false, std::memory_order_release);
    return true;
}

void DatabaseSwitchWorkerRuntime::Wait() {
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

} // namespace aura::database_switch
