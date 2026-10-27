#include "AnnotationBridge.h"

#include "nlohmann_json.hpp"

#include <utility>
#include "platform/filesystem.h"

namespace AnnotationBridge {

namespace {

nlohmann::json ToJson(const ThemeInspector::AnnotationFrameSnapshot& snapshot) {
    nlohmann::json zones = nlohmann::json::array();
    for (const auto& zone : snapshot.zones) {
        zones.push_back({
            {"stableId", zone.stableId},
            {"key", zone.key},
            {"displayName", zone.displayName},
            {"sourceAnchor", zone.sourceAnchor},
            {"minX", zone.minX},
            {"minY", zone.minY},
            {"maxX", zone.maxX},
            {"maxY", zone.maxY},
            {"area", zone.area},
            {"order", zone.order},
        });
    }
    return {
        {"schemaVersion", 1},
        {"frameNumber", snapshot.frameNumber},
        {"displayWidth", snapshot.displayWidth},
        {"displayHeight", snapshot.displayHeight},
        {"zones", std::move(zones)},
    };
}

bool ReplaceFile(const std::filesystem::path& temporary,
                 const std::filesystem::path& destination) {
    return Platform::FileSystem::AtomicFileReplace(
        destination.u8string(), temporary.u8string());
}

} // namespace

Publisher::~Publisher() {
    Stop();
}

bool Publisher::Start(const std::filesystem::path& outputDirectory, double maxHz) {
    Stop();
    if (outputDirectory.empty() || maxHz <= 0.0)
        return false;

    if (!Platform::FileSystem::EnsureDirectoryExists(outputDirectory.u8string()))
        return false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        outputDirectory_ = outputDirectory;
        minimumInterval_ = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / maxHz));
        stopRequested_ = false;
        hasPendingSnapshot_ = false;
        running_ = true;
    }
    worker_ = std::thread(&Publisher::Run, this);
    return true;
}

void Publisher::Submit(ThemeInspector::AnnotationFrameSnapshot snapshot) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stopRequested_)
            return;
        latestSnapshot_ = std::move(snapshot);
        hasPendingSnapshot_ = true;
    }
    condition_.notify_one();
}

void Publisher::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ && !worker_.joinable())
            return;
        stopRequested_ = true;
    }
    condition_.notify_one();
    if (worker_.joinable())
        worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    stopRequested_ = false;
    hasPendingSnapshot_ = false;
}

bool Publisher::IsRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_ && !stopRequested_;
}

void Publisher::Run() {
    auto nextPublishAt = std::chrono::steady_clock::time_point::min();
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        condition_.wait(lock, [this] { return stopRequested_ || hasPendingSnapshot_; });
        if (stopRequested_ && !hasPendingSnapshot_)
            break;

        if (!stopRequested_ && nextPublishAt != std::chrono::steady_clock::time_point::min()) {
            condition_.wait_until(lock, nextPublishAt, [this] { return stopRequested_; });
        }

        ThemeInspector::AnnotationFrameSnapshot snapshot = latestSnapshot_;
        hasPendingSnapshot_ = false;
        lock.unlock();
        Publish(snapshot);
        nextPublishAt = std::chrono::steady_clock::now() + minimumInterval_;
        lock.lock();
    }
    running_ = false;
}

bool Publisher::Publish(const ThemeInspector::AnnotationFrameSnapshot& snapshot) const {
    const std::filesystem::path temporary = outputDirectory_ / "zones.json.tmp";
    const std::filesystem::path destination = outputDirectory_ / "zones.json";
    if (!Platform::FileSystem::WriteFileText(temporary.u8string(), ToJson(snapshot).dump()))
        return false;
    if (ReplaceFile(temporary, destination))
        return true;
    Platform::FileSystem::RemoveFile(temporary.u8string());
    return false;
}

} // namespace AnnotationBridge
