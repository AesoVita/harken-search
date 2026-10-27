#pragma once

#include "ThemeInspector.h"

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

// @stage7-contract
// API contract: Publisher owns its worker thread and publishes the latest
// submitted snapshot to the caller-selected directory. Start/Stop are
// lifecycle operations on the owning application thread; Submit copies the
// snapshot and may be called by producers while running. IsRunning is a
// lock-safe observation; callers must stop before destruction and treat a
// false Start as a filesystem/lifecycle failure.
namespace AnnotationBridge {

class Publisher {
public:
    Publisher() = default;
    ~Publisher();

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

    bool Start(const std::filesystem::path& outputDirectory, double maxHz = 10.0);
    void Submit(ThemeInspector::AnnotationFrameSnapshot snapshot);
    void Stop();
    bool IsRunning() const;

private:
    void Run();
    bool Publish(const ThemeInspector::AnnotationFrameSnapshot& snapshot) const;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    std::filesystem::path outputDirectory_;
    std::chrono::steady_clock::duration minimumInterval_{};
    ThemeInspector::AnnotationFrameSnapshot latestSnapshot_;
    bool running_ = false;
    bool stopRequested_ = false;
    bool hasPendingSnapshot_ = false;
};

} // namespace AnnotationBridge
