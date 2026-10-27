#pragma once

#include "SelectedWaveformRebuildQueue.h"
#include "WaveformRebuildRequestSubmission.h"

#include <cstddef>
#include <string>
#include <vector>

namespace aura::waveform_rebuild {

// @stage7-contract
// Private UI-thread workflow contract. The workflow owns its copied queue and
// counters; all ports are borrowed synchronously and retained nowhere.
enum class SelectedWaveformRebuildPumpDecision {
    NoWork,
    BlockedByExplicitRebuild,
    ClearedClosedDatabase,
    Pumped,
    Completed,
};

enum class SelectedWaveformRebuildQueueDecision {
    Queued,
    AlreadyRunning,
    MissingFile,
    Invalid,
};

enum class SelectedWaveformRebuildLifecycle {
    Idle,
    Running,
    Completed,
    Failed,
    Cancelled,
};

struct SelectedWaveformRebuildLifecycleState {
    SelectedWaveformRebuildLifecycle state =
        SelectedWaveformRebuildLifecycle::Idle;
    std::string filePath;
    std::string message;
};

struct SelectedWaveformRebuildStats {
    int cleared = 0;
    int queued = 0;
    int pending = 0;
    std::size_t remaining = 0;
};

struct SelectedWaveformRebuildPumpRequest {
    bool explicitRebuildActive = false;
    bool databaseOpen = false;
};

struct SelectedWaveformRebuildPumpResult {
    SelectedWaveformRebuildPumpDecision decision =
        SelectedWaveformRebuildPumpDecision::NoWork;
    SelectedWaveformRebuildStats stats;
};

class SelectedWaveformRebuildStoragePort {
public:
    virtual ~SelectedWaveformRebuildStoragePort() = default;

    virtual bool beginTransaction() = 0;
    virtual bool clearWaveformCacheByPath(const std::string& filePath) = 0;
    virtual bool commitTransaction() = 0;
    virtual void rollbackTransaction() = 0;
    virtual bool miniOnlyPolicy() const = 0;
};

class SelectedWaveformRebuildEffectsPort {
public:
    virtual ~SelectedWaveformRebuildEffectsPort() = default;

    virtual std::string loadedFilePath() const = 0;
    virtual void clearWaveformUiCachesForPath(const std::string& filePath) = 0;
    virtual void addPendingWaveform(
        const std::string& filePath,
        bool detail) = 0;
    virtual bool isLoadedItem(
        const SelectedWaveformRebuildItem& item,
        const std::string& loadedPath) const = 0;
    virtual void forceLoadedItem(const SelectedWaveformRebuildItem& item) = 0;
    virtual void reportTransactionCommitFailure() = 0;
    virtual void reportForcedCurrent(
        const SelectedWaveformRebuildItem& item) = 0;
    virtual void reportCompleted(
        const SelectedWaveformRebuildStats& stats) = 0;
};

class SelectedWaveformRebuildWorkflow {
public:
    void start(const std::vector<SelectedWaveformRebuildItem>& items);
    SelectedWaveformRebuildQueueDecision queueCurrentFile(
        const SelectedWaveformRebuildItem& item,
        bool fileExists);
    void cancel();
    void shutdown();
    void clear();
    bool active() const noexcept;
    bool currentFileTaskActive() const noexcept;
    std::size_t remaining() const noexcept;
    SelectedWaveformRebuildStats stats() const noexcept;
    SelectedWaveformRebuildLifecycleState lifecycle() const noexcept;
    bool observeWorkerResult(
        const std::string& filePath,
        bool success,
        const std::string& errorCode);

    SelectedWaveformRebuildPumpResult pump(
        SelectedWaveformRebuildStoragePort& storage,
        SelectedWaveformRebuildEffectsPort& effects,
        WaveformRebuildSubmissionPort& submissions,
        const SelectedWaveformRebuildPumpRequest& request);

private:
    void setLifecycle(
        SelectedWaveformRebuildLifecycle state,
        std::string filePath,
        std::string message);
    void resetCurrentFileTask();

    SelectedWaveformRebuildQueue queue_;
    int cleared_ = 0;
    int queued_ = 0;
    int pending_ = 0;
    bool currentFileTaskActive_ = false;
    std::string currentFilePath_;
    SelectedWaveformRebuildLifecycleState lifecycle_;
};

}  // namespace aura::waveform_rebuild
