#pragma once

#include <functional>
#include <memory>
#include <string>

namespace aura::file_player {

enum class DeferredDatabaseWarmupStartResult {
    Started,
    AlreadyStarted,
    PathUnavailable,
};

using DeferredDatabaseWarmupOperation =
    std::function<bool(const std::string& dbPathUtf8)>;

class DeferredDatabaseWarmupRuntime {
public:
    explicit DeferredDatabaseWarmupRuntime(
        DeferredDatabaseWarmupOperation operation);
    ~DeferredDatabaseWarmupRuntime();

    DeferredDatabaseWarmupRuntime(const DeferredDatabaseWarmupRuntime&) = delete;
    DeferredDatabaseWarmupRuntime& operator=(const DeferredDatabaseWarmupRuntime&) = delete;

    DeferredDatabaseWarmupStartResult Start(std::string dbPathUtf8);
    bool PollFinished(bool& success);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool WarmFilePlayerDatabaseReadOnly(const std::string& dbPathUtf8);

} // namespace aura::file_player
