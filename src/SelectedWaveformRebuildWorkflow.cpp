#include "SelectedWaveformRebuildWorkflow.h"

#include <utility>
#include <vector>

namespace aura::waveform_rebuild {
namespace {

constexpr std::size_t kPerFrame = 4;

}  // namespace

void SelectedWaveformRebuildWorkflow::start(
    const std::vector<SelectedWaveformRebuildItem>& items) {
    queue_.start(items);
    cleared_ = 0;
    queued_ = 0;
    pending_ = 0;
    currentFileTaskActive_ = false;
    currentFilePath_.clear();
}

SelectedWaveformRebuildQueueDecision SelectedWaveformRebuildWorkflow::queueCurrentFile(
    const SelectedWaveformRebuildItem& item,
    bool fileExists) {
    if (item.filePath.empty()) {
        setLifecycle(
            SelectedWaveformRebuildLifecycle::Failed,
            {},
            "empty");
        return SelectedWaveformRebuildQueueDecision::Invalid;
    }
    if (currentFileTaskActive_ && currentFilePath_ == item.filePath) {
        return SelectedWaveformRebuildQueueDecision::AlreadyRunning;
    }
    if (!fileExists) {
        resetCurrentFileTask();
        setLifecycle(
            SelectedWaveformRebuildLifecycle::Failed,
            item.filePath,
            "missing");
        return SelectedWaveformRebuildQueueDecision::MissingFile;
    }

    if (!queue_.enqueue(item)) {
        return SelectedWaveformRebuildQueueDecision::AlreadyRunning;
    }
    currentFileTaskActive_ = true;
    currentFilePath_ = item.filePath;
    setLifecycle(
        SelectedWaveformRebuildLifecycle::Running,
        item.filePath,
        "running");
    return SelectedWaveformRebuildQueueDecision::Queued;
}

void SelectedWaveformRebuildWorkflow::cancel() {
    const std::string path = currentFilePath_;
    resetCurrentFileTask();
    setLifecycle(
        SelectedWaveformRebuildLifecycle::Cancelled,
        path,
        "cancelled");
}

void SelectedWaveformRebuildWorkflow::shutdown() {
    resetCurrentFileTask();
    setLifecycle(SelectedWaveformRebuildLifecycle::Idle, {}, {});
}

void SelectedWaveformRebuildWorkflow::clear() {
    queue_.clear();
    currentFileTaskActive_ = false;
    currentFilePath_.clear();
    setLifecycle(SelectedWaveformRebuildLifecycle::Idle, {}, {});
}

void SelectedWaveformRebuildWorkflow::setLifecycle(
    SelectedWaveformRebuildLifecycle state,
    std::string filePath,
    std::string message) {
    lifecycle_.state = state;
    lifecycle_.filePath = std::move(filePath);
    lifecycle_.message = std::move(message);
}

void SelectedWaveformRebuildWorkflow::resetCurrentFileTask() {
    queue_.clear();
    currentFileTaskActive_ = false;
    currentFilePath_.clear();
}

bool SelectedWaveformRebuildWorkflow::active() const noexcept {
    return queue_.active();
}

bool SelectedWaveformRebuildWorkflow::currentFileTaskActive() const noexcept {
    return currentFileTaskActive_;
}

std::size_t SelectedWaveformRebuildWorkflow::remaining() const noexcept {
    return queue_.remaining();
}

SelectedWaveformRebuildStats SelectedWaveformRebuildWorkflow::stats() const noexcept {
    return {cleared_, queued_, pending_, queue_.remaining()};
}

SelectedWaveformRebuildLifecycleState
SelectedWaveformRebuildWorkflow::lifecycle() const noexcept {
    return lifecycle_;
}

bool SelectedWaveformRebuildWorkflow::observeWorkerResult(
    const std::string& filePath,
    bool success,
    const std::string& errorCode) {
    if (!currentFileTaskActive_ || filePath.empty() || filePath != currentFilePath_) {
        return false;
    }
    const std::string path = currentFilePath_;
    currentFileTaskActive_ = false;
    currentFilePath_.clear();
    if (success) {
        setLifecycle(
            SelectedWaveformRebuildLifecycle::Completed,
            path,
            "completed");
    } else {
        setLifecycle(
            SelectedWaveformRebuildLifecycle::Failed,
            path,
            errorCode.empty() ? "failed" : errorCode);
    }
    return true;
}

SelectedWaveformRebuildPumpResult SelectedWaveformRebuildWorkflow::pump(
    SelectedWaveformRebuildStoragePort& storage,
    SelectedWaveformRebuildEffectsPort& effects,
    WaveformRebuildSubmissionPort& submissions,
    const SelectedWaveformRebuildPumpRequest& request) {
    SelectedWaveformRebuildPumpResult result;
    result.stats = stats();
    if (!queue_.active()) return result;

    if (request.explicitRebuildActive) {
        result.decision =
            SelectedWaveformRebuildPumpDecision::BlockedByExplicitRebuild;
        return result;
    }

    if (!request.databaseOpen) {
        if (currentFileTaskActive_) {
            cancel();
        } else {
            queue_.clear();
        }
        result.decision =
            SelectedWaveformRebuildPumpDecision::ClearedClosedDatabase;
        result.stats = stats();
        return result;
    }

    std::vector<SelectedWaveformRebuildItem> processedItems;
    processedItems.reserve(kPerFrame);

    const bool transactionBegan = storage.beginTransaction();
    queue_.pump(kPerFrame, [&](const SelectedWaveformRebuildItem& item) {
        if (storage.clearWaveformCacheByPath(item.filePath)) {
            ++cleared_;
        }
        processedItems.push_back(item);
    });
    if (transactionBegan && !storage.commitTransaction()) {
        storage.rollbackTransaction();
        effects.reportTransactionCommitFailure();
    }

    const std::string loadedPath = effects.loadedFilePath();
    const bool miniOnly = storage.miniOnlyPolicy();
    bool forcedCurrent = false;

    for (const auto& item : processedItems) {
        effects.clearWaveformUiCachesForPath(item.filePath);

        const auto submission = SubmitWaveformRebuildRequest(
            submissions,
            {item.filePath, item.id, miniOnly});
        if (submission == WaveformRebuildSubmissionResult::Submitted) {
            ++queued_;
        } else {
            effects.addPendingWaveform(item.filePath, !miniOnly);
            ++pending_;
        }

        const bool isCurrentLoadedItem =
            !loadedPath.empty() && effects.isLoadedItem(item, loadedPath);
        if (!forcedCurrent && isCurrentLoadedItem) {
            effects.forceLoadedItem(item);
            effects.reportForcedCurrent(item);
            forcedCurrent = true;
        }
    }

    result.stats = stats();
    if (!queue_.active()) {
        result.decision = SelectedWaveformRebuildPumpDecision::Completed;
        effects.reportCompleted(result.stats);
    } else {
        result.decision = SelectedWaveformRebuildPumpDecision::Pumped;
    }
    return result;
}

}  // namespace aura::waveform_rebuild
