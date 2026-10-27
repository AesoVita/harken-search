#pragma once

#include "BrowserSystemFolderSearchPolicy.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aura::browser {

struct SystemFolderSearchRoot {
    std::string path;
    std::string displayName;
};

struct SystemFolderSearchSnapshot {
    uint64_t generation = 0;
    size_t visited = 0;
    bool truncated = false;
    bool ready = false;
    std::vector<SystemFolderSearchHit> results;
};

inline constexpr std::chrono::milliseconds kSystemFolderSearchPublishInterval{120};

class SystemFolderSearchRuntime {
public:
    SystemFolderSearchRuntime();
    ~SystemFolderSearchRuntime();

    SystemFolderSearchRuntime(const SystemFolderSearchRuntime&) = delete;
    SystemFolderSearchRuntime& operator=(const SystemFolderSearchRuntime&) = delete;

    uint64_t Start(
        std::string query,
        std::vector<SystemFolderSearchRoot> roots);
    void Cancel();
    bool Poll(SystemFolderSearchSnapshot& snapshot);
    size_t VisitedCount() const;
    bool Busy() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aura::browser
