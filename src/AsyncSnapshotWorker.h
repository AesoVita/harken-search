#pragma once

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace HarkenSearch {

// @stage7-contract
// API contract: AsyncSnapshotWorker owns one persistent thread and its copied
// Handler. Submit takes ownership of a request only when it returns true;
// Poll transfers a completed Result or rethrows the handler exception on the
// polling thread. Destruction cancels pending work and joins; callers must not
// race object destruction with Submit/Poll.

// Runs one immutable request snapshot at a time on a persistent worker thread.
// Poll() has an atomic fast path so render-loop callers never wait while no
// result is ready.
template <typename Request, typename Result>
class AsyncSnapshotWorker final {
public:
    using Handler = std::function<Result(Request)>;

    explicit AsyncSnapshotWorker(Handler handler)
        : handler_(std::move(handler)), worker_([this]() { Run(); }) {}

    ~AsyncSnapshotWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            pending_.reset();
        }
        wake_.notify_one();
        if (worker_.joinable()) worker_.join();
    }

    AsyncSnapshotWorker(const AsyncSnapshotWorker&) = delete;
    AsyncSnapshotWorker& operator=(const AsyncSnapshotWorker&) = delete;

    bool Submit(Request request) {
        bool expected = false;
        if (!busy_.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                busy_.store(false, std::memory_order_release);
                return false;
            }
            pending_.emplace(std::move(request));
        }
        wake_.notify_one();
        return true;
    }

    std::optional<Result> Poll() {
        if (!completionReady_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        std::optional<Result> result;
        std::exception_ptr failure;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!completionReady_.load(std::memory_order_relaxed)) {
                return std::nullopt;
            }
            result = std::move(completed_);
            failure = completedFailure_;
            completed_.reset();
            completedFailure_ = nullptr;
            completionReady_.store(false, std::memory_order_release);
            busy_.store(false, std::memory_order_release);
        }

        if (failure) std::rethrow_exception(failure);
        return result;
    }

    bool IsBusy() const noexcept {
        return busy_.load(std::memory_order_acquire);
    }

private:
    void Run() {
        for (;;) {
            std::optional<Request> request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this]() {
                    return stopping_ || pending_.has_value();
                });
                if (stopping_) return;
                request.emplace(std::move(*pending_));
                pending_.reset();
            }

            std::optional<Result> result;
            std::exception_ptr failure;
            try {
                result.emplace(handler_(std::move(*request)));
            } catch (...) {
                failure = std::current_exception();
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (stopping_) return;
                completed_ = std::move(result);
                completedFailure_ = failure;
                completionReady_.store(true, std::memory_order_release);
            }
        }
    }

    Handler handler_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<Request> pending_;
    std::optional<Result> completed_;
    std::exception_ptr completedFailure_;
    bool stopping_ = false;
    std::atomic<bool> busy_{false};
    std::atomic<bool> completionReady_{false};
    std::thread worker_;
};

} // namespace HarkenSearch
