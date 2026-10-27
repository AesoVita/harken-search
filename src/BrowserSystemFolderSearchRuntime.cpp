#include "BrowserSystemFolderSearchRuntime.h"

#include "PathUtils.h"
#include "platform/filesystem.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

namespace aura::browser {

namespace {

std::string BaseNameUtf8(const std::string& path) {
    if (path.empty()) return {};
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos
        ? path
        : path.substr(separator + 1);
}

std::vector<std::string> SplitQuery(const std::string& query) {
    std::vector<std::string> terms;
    std::string current;
    for (const unsigned char ch : query) {
        if (std::isspace(ch)) {
            if (!current.empty()) {
                terms.push_back(std::move(current));
                current.clear();
            }
        } else {
            current.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    if (!current.empty()) terms.push_back(std::move(current));
    return terms;
}

} // namespace

struct SystemFolderSearchRuntime::Impl {
    struct Task {
        std::atomic<bool> ready{false};
        std::atomic<bool> dirty{false};
        std::atomic<size_t> visited{0};
        mutable std::mutex mutex;
        uint64_t generation = 0;
        bool truncated = false;
        std::vector<SystemFolderSearchHit> results;
    };

    std::shared_ptr<std::atomic<bool>> cancel;
    std::shared_ptr<Task> task;
    uint64_t generation = 0;
    uint64_t appliedGeneration = 0;
    bool busy = false;
};

SystemFolderSearchRuntime::SystemFolderSearchRuntime()
    : impl_(std::make_unique<Impl>()) {}

SystemFolderSearchRuntime::~SystemFolderSearchRuntime() {
    Cancel();
}

uint64_t SystemFolderSearchRuntime::Start(
    std::string query,
    std::vector<SystemFolderSearchRoot> roots) {
    Cancel();

    auto cancel = std::make_shared<std::atomic<bool>>(false);
    auto task = std::make_shared<Impl::Task>();
    task->generation = ++impl_->generation;
    impl_->cancel = cancel;
    impl_->task = task;
    impl_->busy = true;

    std::thread([query = std::move(query),
                 roots = std::move(roots),
                 cancel,
                 task]() {
        std::vector<SystemFolderSearchHit> hits;
        std::unordered_set<std::string> seenHits;
        hits.reserve(64);
        const auto terms = SplitQuery(query);
        size_t visited = 0;
        bool truncated = false;
        auto lastPublishAt = std::chrono::steady_clock::now();

        auto publish = [&](bool force) {
            const auto now = std::chrono::steady_clock::now();
            if (!force &&
                now - lastPublishAt < kSystemFolderSearchPublishInterval) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(task->mutex);
                task->results = hits;
                task->truncated = truncated;
            }
            task->visited.store(visited, std::memory_order_relaxed);
            task->dirty.store(true, std::memory_order_release);
            lastPublishAt = now;
        };

        auto addFolderHit = [&](const std::string& folderPath,
                                const std::string& displayName) {
            const std::string canonical = HarkenSearch::CanonicalFolderPath(folderPath);
            if (canonical.empty() || seenHits.count("d:" + canonical) > 0) {
                return false;
            }
            seenHits.insert("d:" + canonical);
            std::string name = displayName;
            if (name.empty()) {
                name = BaseNameUtf8(folderPath);
            }
            if (name.empty()) name = folderPath;
            hits.push_back({folderPath, name, folderPath, false});
            publish(false);
            return true;
        };

        for (const auto& root : roots) {
            if (cancel->load(std::memory_order_relaxed)) break;
            try {
                const std::string rootName =
                    root.displayName.empty() ? root.path : root.displayName;
                if (MatchesSystemFolderSearchQuery(rootName, root.path, terms)) {
                    addFolderHit(root.path, rootName);
                }

                Platform::FileSystem::DirectoryIterator iterator;
                if (!iterator.Open(root.path, true, true)) continue;
                Platform::FileSystem::DirectoryEntry entry;
                std::string iteratorError;
                while (iterator.Next(entry, &iteratorError) ==
                           Platform::FileSystem::DirectoryReadStatus::Entry &&
                       !cancel->load(std::memory_order_relaxed)) {
                    ++visited;
                    if (entry.isDirectory) {
                        std::string childName;
                        childName = BaseNameUtf8(entry.pathUtf8);
                        if (childName.empty()) childName = entry.pathUtf8;
                        if (MatchesSystemFolderSearchQuery(
                                childName, entry.pathUtf8, terms)) {
                            addFolderHit(entry.pathUtf8, childName);
                        }
                    } else if (entry.isRegularFile) {
                        std::string fileName;
                        fileName = BaseNameUtf8(entry.pathUtf8);
                        if (fileName.empty()) fileName = entry.pathUtf8;
                        if (MatchesSystemFolderSearchQuery(
                                fileName, entry.pathUtf8, terms)) {
                            const std::string parentPath =
                                HarkenSearch::DisplayFolderFromFilePath(entry.pathUtf8);
                            std::string parentName = BaseNameUtf8(parentPath);
                            if (parentName.empty()) parentName = parentPath;
                            addFolderHit(parentPath, parentName);
                        }
                    }
                    publish(false);
                }
            } catch (...) {
            }
        }

        std::sort(hits.begin(), hits.end(),
                  [](const SystemFolderSearchHit& a,
                     const SystemFolderSearchHit& b) {
                      std::string aLower = a.displayName;
                      std::string bLower = b.displayName;
                      std::transform(
                          aLower.begin(), aLower.end(), aLower.begin(), ::tolower);
                      std::transform(
                          bLower.begin(), bLower.end(), bLower.begin(), ::tolower);
                      return aLower == bLower ? a.path < b.path : aLower < bLower;
                  });

        {
            std::lock_guard<std::mutex> lock(task->mutex);
            task->results = std::move(hits);
            task->truncated = truncated;
        }
        task->visited.store(visited, std::memory_order_relaxed);
        task->ready.store(true, std::memory_order_release);
        task->dirty.store(true, std::memory_order_release);
    }).detach();

    return task->generation;
}

void SystemFolderSearchRuntime::Cancel() {
    if (impl_->cancel) {
        impl_->cancel->store(true, std::memory_order_relaxed);
    }
    impl_->cancel.reset();
    impl_->task.reset();
    impl_->busy = false;
}

bool SystemFolderSearchRuntime::Poll(SystemFolderSearchSnapshot& snapshot) {
    if (!impl_->task) return false;
    const auto task = impl_->task;
    const bool ready = task->ready.load(std::memory_order_acquire);
    const bool dirty = task->dirty.exchange(false, std::memory_order_acq_rel);
    if (!ready && !dirty) return false;

    bool applied = false;
    if (task->generation >= impl_->appliedGeneration) {
        impl_->appliedGeneration = task->generation;
        snapshot.generation = task->generation;
        snapshot.visited = task->visited.load(std::memory_order_relaxed);
        snapshot.ready = ready;
        {
            std::lock_guard<std::mutex> lock(task->mutex);
            snapshot.results = task->results;
            snapshot.truncated = task->truncated;
        }
        applied = true;
    }
    if (ready && impl_->task == task) {
        impl_->task.reset();
        impl_->cancel.reset();
        impl_->busy = false;
    }
    return applied;
}

size_t SystemFolderSearchRuntime::VisitedCount() const {
    return impl_->task
        ? impl_->task->visited.load(std::memory_order_relaxed)
        : 0;
}

bool SystemFolderSearchRuntime::Busy() const {
    return impl_->busy;
}

} // namespace aura::browser
