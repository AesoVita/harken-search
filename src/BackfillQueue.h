// File-ID queue for durable cover, L1 waveform, and metadata backfill jobs.
//
// Database path and generation isolate runtime state across database switches.
// Jobs use file IDs rather than paths to keep memory bounded for large libraries.
//
// The caller stops the worker pool, closes the previous database, opens the
// next database, installs its queue identity, and rebuilds backfill_pending.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Database;

namespace aura {

// @stage7-contract
// API contract: BackfillQueue owns durable queue identity and uses file IDs for
// bounded runtime state. Callers establish database identity before enqueue or
// refill, serialize database mutation handoff through the declared mutex, and
// treat returned snapshots as copies rather than retained database storage.

// Serializes the tiny database snapshot/clear handoff used by the worker
// refill path. It prevents a stale pending-row snapshot from re-arming a
// runtime terminal entry after another worker has already cleared the row.
std::mutex& BackfillDatabaseMutationMutex();

enum class BackfillJobKind : uint8_t {
    CoverResolveAndThumb = 0,
    WaveformL1Overview   = 1,
    MetadataRefresh      = 2,
};

constexpr int kBackfillJobKindCount = 3;

enum class BackfillPriority : uint8_t {
    SelectedFile = 0,
    VisibleRow   = 1,
    RecentScan   = 2,
    IdleBackfill = 3,
};

constexpr int kBackfillPriorityCount = 4;

enum class BackfillItemState : uint8_t {
    Unknown          = 0,
    Pending          = 1,
    InFlight         = 2,
    Cached           = 3,
    FailedTransient  = 4,
    FailedPermanent  = 5,
};

enum class BackfillCompletionOutcome : uint8_t {
    Success           = 0,
    TransientFailure  = 1,
    PermanentFailure  = 2,
};

// file_id resolves to path via audio_files at dequeue time.
struct BackfillJob {
    int64_t fileId;          // PK in audio_files
    int64_t dbGeneration;    // stale if != queue's current
    int64_t enqueuedAt;      // ms epoch
    int64_t scanSessionId;   // 0 for non-foreground-scan work
    BackfillJobKind kind;
    BackfillPriority priority;
    uint8_t _padding[6];     // explicit padding; keeps deterministic size
};
static_assert(sizeof(BackfillJob) <= 40, "BackfillJob memory budget exceeded");

class BackfillQueue {
public:
    BackfillQueue();
    ~BackfillQueue();

    BackfillQueue(const BackfillQueue&) = delete;
    BackfillQueue& operator=(const BackfillQueue&) = delete;

// DB lifecycle (Card 1 follow-up commit, declared for Card 2/3/4 ABI)
    void onDbOpened(std::string dbPath, int64_t newGeneration);
    void onDbClosing(int64_t closingGeneration);
    void rebuildFromDatabase(Database* db, int windowSize = 4096);
    void refillFromDatabaseIfBelowWaterMark(Database* db,
                                            int lowWaterMark = 5000,
                                            int refillBatch  = 20000);

// Runtime enqueue paths (this slice owns the mechanics)

    // Generic enqueue. Stamps the job with the queue's current generation.
    // Deduplicates on (fileId, kind): a second enqueue for the same key is a
    // no-op unless `priority` is *higher* (lower enum value), in which case
    // the existing job is promoted.
    void enqueue(int64_t fileId, BackfillJobKind kind, BackfillPriority priority);

    // Scan-thread enqueue with snapshot guard. The durable pending row must
    // already be committed. Runtime scan work is capped to a bounded hot
    // window; overflow remains durable for worker low-water refill. Drops
    // silently if the queue's
    // current (dbPath, dbGeneration) no longer matches the snapshot the
    // scanner captured at scan-start. Use this on the scan path; use plain
    // enqueue() everywhere else.
    void enqueueFromScan(int64_t fileId,
                         BackfillJobKind kind,
                         BackfillPriority priority,
                         const std::string& expectedDbPath,
                         int64_t expectedDbGeneration,
                         int64_t scanSessionId = 0);

    // Blocks until a job is available or shutdown was called. Returns false
    // on shutdown.
    bool dequeue(BackfillJob& out);

    // Non-blocking variant for the UI pump. Returns false when no live job is
    // immediately available or shutdown was called.
    bool tryDequeue(BackfillJob& out);
    bool tryDequeueKind(BackfillJobKind kind, BackfillJob& out);

    // Promote an already-queued job to a higher priority (lower enum value).
    // No-op if the job is not currently queued or the new priority is not
    // higher than the existing one.
    void promote(int64_t fileId, BackfillPriority newPriority);

    // Worker reports the outcome of a dequeued job. Drives cursor advance
    // and requeue policy per the v6 failure-semantics contract.
    void reportOutcome(const BackfillJob& job, BackfillCompletionOutcome outcome);

    // UI render code queries this. Workers MUST NOT draw based on this — they
    // only report state via reportOutcome().
    BackfillItemState itemState(int64_t fileId, BackfillJobKind kind) const;
    std::vector<BackfillJob> markStaleInFlightPermanent(int64_t staleAfterMs);

    struct Snapshot {
        int totalEnqueued;       // lifetime count of successful enqueues
        int done;                // success + permanent-failure
        int inFlight;            // dequeued but not yet reported
        int byKind[kBackfillJobKindCount];        // current queued count per kind
        int byPriority[kBackfillPriorityCount];   // current queued count per priority
        int scanStaleRejected;   // enqueueFromScan drops due to gen mismatch
        int pendingTableRows;    // last refill estimate; -1 if never queried
    };
    Snapshot snapshot() const;

    void shutdown();

    // For test/diagnostics: expose the current generation so callers can
    // assert their snapshot is still valid.
    int64_t currentGeneration() const;
    std::string currentDbPath() const;

private:
    // Caller-holds-lock enqueue. Used by both `enqueue` (after generic
    // shutdown check) and `enqueueFromScan` (after snapshot validation),
    // ensuring the validation + stamp pair is atomic w.r.t. DB switches.
    void enqueueLocked(int64_t fileId,
                       BackfillJobKind kind,
                       BackfillPriority priority,
                       int64_t scanSessionId,
                       bool durablePending = false);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}
