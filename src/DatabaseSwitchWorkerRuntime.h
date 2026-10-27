#pragma once

#include <functional>
#include <memory>

namespace aura::database_switch {

using DatabaseSwitchOperation = std::function<bool()>;

class DatabaseSwitchWorkerRuntime {
public:
    DatabaseSwitchWorkerRuntime();
    ~DatabaseSwitchWorkerRuntime();

    DatabaseSwitchWorkerRuntime(const DatabaseSwitchWorkerRuntime&) = delete;
    DatabaseSwitchWorkerRuntime& operator=(
        const DatabaseSwitchWorkerRuntime&) = delete;

    void Start(DatabaseSwitchOperation operation);
    bool PollFinished(bool& success);
    void Wait();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aura::database_switch
