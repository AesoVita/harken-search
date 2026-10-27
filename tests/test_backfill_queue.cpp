// test_backfill_queue.cpp — slice7-A (F-001) Card 1 runtime queue tests.
//
// Scope: runtime queue mechanics only. DB lifecycle + worker DB connection +
// windowed recovery tests are in test_backfill_pending_recovery.cpp,
// test_enqueue_from_scan_stale_gen.cpp, test_backfill_worker_db_lifecycle.cpp,
// and test_backfill_failure_semantics.cpp — those land with the next commit
// inside this card (DB-backed implementation).
//
// What this test verifies for the queue-mechanics-only commit:
//   1. BackfillJob is at most 32 bytes (static_assert backup)
//   2. enqueue + dequeue round-trip on a single (fileId, kind)
//   3. Dedupe — enqueueing the same key at equal-or-lower priority is a no-op
//   4. Promote — enqueueing at a higher priority promotes the existing entry
//   5. Priority ordering across two enqueues with different priorities
//   6. FIFO within the same priority class
//   7. shutdown() makes blocked dequeue() return false within 100 ms
//   8. enqueueFromScan with mismatched (dbPath, dbGen) silently drops and
//      bumps scanStaleRejected
//   9. reportOutcome(Success) — state becomes Cached, inFlight decrements
//  10. reportOutcome(TransientFailure) — entry re-queues at RecentScan
//  11. reportOutcome(TransientFailure) ×3 — auto-converts to FailedPermanent
//  12. Multi-producer + multi-consumer stress — 4P + 4C × 1000 jobs, no dup,
//      no loss, FIFO within priority (best-effort under contention)

#include "BackfillQueue.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace aura;

namespace {

int g_passed = 0;
int g_failed = 0;

#define CHECK(cond, desc)                                                     \
    do {                                                                      \
        if (cond) {                                                           \
            ++g_passed;                                                       \
            std::printf("  PASS: %s\n", desc);                                \
        } else {                                                              \
            ++g_failed;                                                       \
            std::printf("  FAIL: %s (line %d)\n", desc, __LINE__);            \
        }                                                                     \
    } while (0)

// ── Test 1: job memory budget ─────────────────────────────────────────────
static void test_job_memory_budget() {
    std::printf("\n[1] BackfillJob memory budget\n");
    // The 40-byte budget is intentional: foreground scan completion needs an
    // explicit per-job scanSessionId, not a fileId-only or global-session guess.
    CHECK(sizeof(BackfillJob) <= 40, "sizeof(BackfillJob) <= 40");
    CHECK(sizeof(BackfillJob) == 40, "sizeof(BackfillJob) == 40 with explicit scanSessionId");
}

// ── Test 2: round-trip ────────────────────────────────────────────────────
static void test_round_trip() {
    std::printf("\n[2] enqueue + dequeue round trip\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);
    q.enqueue(42, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);

    BackfillJob j;
    CHECK(q.dequeue(j), "dequeue returns true");
    CHECK(j.fileId == 42, "fileId round-trips");
    CHECK(j.kind == BackfillJobKind::CoverResolveAndThumb, "kind round-trips");
    CHECK(j.priority == BackfillPriority::RecentScan, "priority round-trips");
    CHECK(j.dbGeneration == 1, "dbGeneration stamped to current");

    q.shutdown();
}

// ── Test 3: dedupe at equal-or-lower priority ─────────────────────────────
static void test_dedupe_equal_priority() {
    std::printf("\n[3] dedupe at equal-or-lower priority\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(7, BackfillJobKind::WaveformL1Overview, BackfillPriority::RecentScan);
    q.enqueue(7, BackfillJobKind::WaveformL1Overview, BackfillPriority::RecentScan);  // same — dedupe
    q.enqueue(7, BackfillJobKind::WaveformL1Overview, BackfillPriority::IdleBackfill); // lower priority — dedupe

    auto s = q.snapshot();
    CHECK(s.totalEnqueued == 1, "totalEnqueued == 1 (two later enqueues deduped)");
    CHECK(s.byKind[static_cast<int>(BackfillJobKind::WaveformL1Overview)] == 1,
          "byKind shows exactly 1 queued");

    BackfillJob j;
    CHECK(q.dequeue(j), "first dequeue succeeds");
    CHECK(j.fileId == 7, "got the deduped entry");
    // No more entries should be available — non-blocking probe via shutdown.
    q.shutdown();
    CHECK(!q.dequeue(j), "second dequeue returns false after shutdown");
}

// ── Test 4: promote on higher-priority enqueue ────────────────────────────
static void test_promote_on_higher_enqueue() {
    std::printf("\n[4] promote on higher-priority re-enqueue\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(99, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::IdleBackfill);
    q.enqueue(99, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::SelectedFile);  // promote

    auto s = q.snapshot();
    CHECK(s.byPriority[static_cast<int>(BackfillPriority::IdleBackfill)] == 0,
          "no IdleBackfill queue entries after promote");
    CHECK(s.byPriority[static_cast<int>(BackfillPriority::SelectedFile)] == 1,
          "promoted entry counted at SelectedFile");

    BackfillJob j;
    CHECK(q.dequeue(j), "dequeue succeeds");
    CHECK(j.priority == BackfillPriority::SelectedFile, "dequeued at promoted priority");
    CHECK(j.fileId == 99, "fileId preserved across promote");
    q.shutdown();
}

// ── Test 5: priority ordering across two distinct keys ────────────────────
static void test_priority_ordering() {
    std::printf("\n[5] priority ordering across keys\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    // Push the lower-priority one FIRST so we know the priority — not FIFO —
    // determines dequeue order.
    q.enqueue(10, BackfillJobKind::WaveformL1Overview, BackfillPriority::IdleBackfill);
    q.enqueue(20, BackfillJobKind::WaveformL1Overview, BackfillPriority::SelectedFile);

    BackfillJob j1, j2;
    CHECK(q.dequeue(j1), "first dequeue");
    CHECK(j1.fileId == 20, "SelectedFile drained before IdleBackfill");
    CHECK(q.dequeue(j2), "second dequeue");
    CHECK(j2.fileId == 10, "IdleBackfill drained second");

    q.shutdown();
}

// ── Test 6: FIFO within same priority class ───────────────────────────────
static void test_fifo_within_priority() {
    std::printf("\n[6] FIFO within same priority class\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    for (int i = 1; i <= 5; ++i) {
        q.enqueue(i, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);
    }

    for (int i = 1; i <= 5; ++i) {
        BackfillJob j;
        CHECK(q.dequeue(j), "dequeue succeeds");
        if (j.fileId != i) {
            std::printf("  expected %d, got %lld\n", i, (long long)j.fileId);
            CHECK(false, "FIFO order broken");
        }
    }

    q.shutdown();
}

// ── Test 7: shutdown unblocks waiting dequeue ─────────────────────────────
static void test_shutdown_unblocks_dequeue() {
    std::printf("\n[7] shutdown unblocks waiting dequeue\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    std::atomic<bool> dequeueReturned{false};
    std::atomic<bool> dequeueReturnedTrue{false};

    std::thread waiter([&]() {
        BackfillJob j;
        if (q.dequeue(j)) {
            dequeueReturnedTrue.store(true);
        }
        dequeueReturned.store(true);
    });

    // Give the thread time to block on dequeue.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto t0 = std::chrono::steady_clock::now();
    q.shutdown();

    waiter.join();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    CHECK(dequeueReturned.load(), "waiter thread returned after shutdown");
    CHECK(!dequeueReturnedTrue.load(), "dequeue returned false (not true)");
    CHECK(elapsed < 100, "dequeue unblocked within 100 ms");
    if (elapsed >= 100) {
        std::printf("  observed %lld ms\n", (long long)elapsed);
    }
}

// ── Test 8: enqueueFromScan stale generation drops ────────────────────────
static void test_enqueue_from_scan_stale_drops() {
    std::printf("\n[8] enqueueFromScan with stale generation drops\n");
    BackfillQueue q;
    q.onDbOpened("oldpath.db", 5);

    // Scan thread snapshot is (oldpath.db, 5).
    q.enqueueFromScan(11, BackfillJobKind::CoverResolveAndThumb,
                     BackfillPriority::RecentScan, "oldpath.db", 5);
    auto s1 = q.snapshot();
    CHECK(s1.totalEnqueued == 1, "matching snapshot enqueues");
    CHECK(s1.scanStaleRejected == 0, "no stale reject yet");

    // Simulate a DB switch: generation bump + path change.
    q.onDbOpened("newpath.db", 6);
    q.enqueueFromScan(12, BackfillJobKind::CoverResolveAndThumb,
                     BackfillPriority::RecentScan, "oldpath.db", 5);

    auto s2 = q.snapshot();
    CHECK(s2.totalEnqueued == 1, "stale enqueue did NOT count as success");
    CHECK(s2.scanStaleRejected == 1, "stale enqueue bumped scanStaleRejected");

    // Generation-only mismatch (same path, bumped gen — e.g. schema replay).
    q.enqueueFromScan(13, BackfillJobKind::CoverResolveAndThumb,
                     BackfillPriority::RecentScan, "newpath.db", 5);
    auto s3 = q.snapshot();
    CHECK(s3.scanStaleRejected == 2, "gen-only mismatch also bumps scanStaleRejected");

    // Matching snapshot on the new generation works.
    q.enqueueFromScan(14, BackfillJobKind::CoverResolveAndThumb,
                     BackfillPriority::RecentScan, "newpath.db", 6);
    auto s4 = q.snapshot();
    CHECK(s4.totalEnqueued == 2, "matching enqueue at new gen succeeds");

    q.shutdown();
}

// ── Test 9: reportOutcome Success ─────────────────────────────────────────
static void test_scan_session_id_round_trip() {
    std::printf("\n[8b] scanSessionId round trip\n");

    {
        BackfillQueue q;
        q.onDbOpened("scan-session.db", 11);
        q.enqueueFromScan(123,
                          BackfillJobKind::WaveformL1Overview,
                          BackfillPriority::RecentScan,
                          "scan-session.db",
                          11,
                          1234);

        BackfillJob j;
        CHECK(q.dequeue(j), "scan-session job dequeues");
        CHECK(j.scanSessionId == 1234, "enqueueFromScan preserves scanSessionId");
        q.shutdown();
    }

    {
        BackfillQueue q;
        q.onDbOpened("generic.db", 12);
        q.enqueue(124, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);

        BackfillJob j;
        CHECK(q.dequeue(j), "generic job dequeues");
        CHECK(j.scanSessionId == 0, "generic enqueue keeps scanSessionId zero");
        q.shutdown();
    }

    {
        BackfillQueue q;
        q.onDbOpened("upgrade.db", 13);
        q.enqueue(125, BackfillJobKind::WaveformL1Overview, BackfillPriority::IdleBackfill);
        q.enqueueFromScan(125,
                          BackfillJobKind::WaveformL1Overview,
                          BackfillPriority::RecentScan,
                          "upgrade.db",
                          13,
                          5678);

        BackfillJob j;
        CHECK(q.dequeue(j), "generic-to-scan upgraded job dequeues");
        CHECK(j.scanSessionId == 5678,
              "same-key foreground enqueue upgrades generic scanSessionId");
        q.shutdown();
    }
}

static void test_report_outcome_success() {
    std::printf("\n[9] reportOutcome(Success)\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(50, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);
    BackfillJob j;
    CHECK(q.dequeue(j), "dequeue");
    auto s1 = q.snapshot();
    CHECK(s1.inFlight == 1, "inFlight == 1 after dequeue");
    CHECK(q.itemState(50, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::InFlight,
          "itemState == InFlight after dequeue");

    q.reportOutcome(j, BackfillCompletionOutcome::Success);

    auto s2 = q.snapshot();
    CHECK(s2.inFlight == 0, "inFlight == 0 after Success");
    CHECK(s2.done == 1, "done == 1 after Success");
    CHECK(q.itemState(50, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::Cached,
          "itemState == Cached after Success");

    q.shutdown();
}

// ── Test 10: reportOutcome TransientFailure re-queues ─────────────────────
static void test_report_outcome_transient() {
    std::printf("\n[10] reportOutcome(TransientFailure)\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(60, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::IdleBackfill);
    BackfillJob j;
    CHECK(q.dequeue(j), "dequeue first time");

    q.reportOutcome(j, BackfillCompletionOutcome::TransientFailure);

    auto s = q.snapshot();
    CHECK(s.inFlight == 0, "inFlight cleared after transient");
    CHECK(s.done == 0, "done NOT bumped on transient");
    CHECK(s.byPriority[static_cast<int>(BackfillPriority::RecentScan)] == 1,
          "requeued at RecentScan");
    CHECK(q.itemState(60, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::Pending,
          "itemState back to Pending");

    BackfillJob j2;
    CHECK(q.dequeue(j2), "dequeue retry");
    CHECK(j2.fileId == 60, "same fileId on retry");
    CHECK(j2.priority == BackfillPriority::RecentScan, "retry at RecentScan");

    q.shutdown();
}

// ── Test 11: TransientFailure ×3 auto-converts ────────────────────────────
static void test_report_outcome_transient_retry_budget() {
    std::printf("\n[11] TransientFailure x3 auto-converts to FailedPermanent\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(77, BackfillJobKind::WaveformL1Overview, BackfillPriority::RecentScan);

    for (int attempt = 1; attempt <= 3; ++attempt) {
        BackfillJob j;
        CHECK(q.dequeue(j), "dequeue for retry");
        q.reportOutcome(j, BackfillCompletionOutcome::TransientFailure);
    }

    auto s = q.snapshot();
    CHECK(s.done == 1, "done bumped on auto-convert to permanent");
    CHECK(s.inFlight == 0, "inFlight cleared");
    CHECK(s.byKind[static_cast<int>(BackfillJobKind::WaveformL1Overview)] == 0,
          "no more queued after retry budget exhausted");
    CHECK(q.itemState(77, BackfillJobKind::WaveformL1Overview) == BackfillItemState::FailedPermanent,
          "itemState == FailedPermanent after retry budget");

    q.shutdown();
}

// ── Test 12: multi-producer + multi-consumer ──────────────────────────────
static void test_multi_producer_consumer() {
    std::printf("\n[12] 4 producers + 4 consumers, 4000 jobs total\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    constexpr int kProducers      = 4;
    constexpr int kConsumers      = 4;
    constexpr int kJobsPerProducer = 1000;
    constexpr int kTotalJobs      = kProducers * kJobsPerProducer;

    std::atomic<bool> producersDone{false};
    std::atomic<int>  consumedCount{0};
    std::mutex        consumedMu;
    std::set<int64_t> consumedFileIds;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p]() {
            const int base = p * kJobsPerProducer;
            for (int i = 0; i < kJobsPerProducer; ++i) {
                q.enqueue(base + i + 1, BackfillJobKind::CoverResolveAndThumb,
                          BackfillPriority::RecentScan);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&]() {
            BackfillJob j;
            while (q.dequeue(j)) {
                {
                    std::lock_guard<std::mutex> lk(consumedMu);
                    consumedFileIds.insert(j.fileId);
                }
                // Realistic worker behavior: report Success so the inFlight
                // counter can settle to zero at the end. Also stress-tests
                // reportOutcome under producer+consumer contention.
                q.reportOutcome(j, BackfillCompletionOutcome::Success);
                if (consumedCount.fetch_add(1) + 1 >= kTotalJobs) break;
            }
        });
    }

    for (auto& t : producers) t.join();
    producersDone.store(true);

    // Wait for consumers to drain. With a 5-second budget for 4000 simple ops
    // this should be plenty even under heavy contention.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (consumedCount.load() < kTotalJobs &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    q.shutdown();
    for (auto& t : consumers) t.join();

    CHECK(consumedCount.load() == kTotalJobs, "all 4000 jobs consumed");
    CHECK((int)consumedFileIds.size() == kTotalJobs, "no duplicates across consumers");

    auto s = q.snapshot();
    CHECK(s.totalEnqueued == kTotalJobs, "totalEnqueued matches kTotalJobs");
    CHECK(s.inFlight == 0, "inFlight settles to 0 after all consumers reportOutcome");
    CHECK(s.done == kTotalJobs, "done == kTotalJobs after all Success reports");
}

// ── Test 13: onDbClosing purges runtime state (Codex v7 P1 #26) ───────────
static void test_on_db_closing_purges_state() {
    std::printf("\n[13] onDbClosing wipes runtime state between DBs\n");
    BackfillQueue q;

    // Phase 1: DB-A. Enqueue some, mark some as Cached.
    q.onDbOpened("dbA.db", 1);
    q.enqueue(5, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);
    q.enqueue(6, BackfillJobKind::WaveformL1Overview,   BackfillPriority::RecentScan);
    BackfillJob j;
    CHECK(q.dequeue(j), "dequeue file_id=5 in DB-A");
    q.reportOutcome(j, BackfillCompletionOutcome::Success);
    CHECK(q.itemState(5, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::Cached,
          "file_id=5 cached in DB-A");
    CHECK(q.itemState(6, BackfillJobKind::WaveformL1Overview) == BackfillItemState::Pending,
          "file_id=6 pending in DB-A");

    // Phase 2: DB switch.
    q.onDbClosing(1);
    auto sClosing = q.snapshot();
    CHECK(sClosing.byKind[0] == 0 && sClosing.byKind[1] == 0,
          "byKind queued counters all zero after onDbClosing");
    CHECK(sClosing.inFlight == 0, "inFlight cleared after onDbClosing");
    CHECK(q.itemState(5, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::Unknown,
          "DB-A file_id=5 state forgotten after onDbClosing");
    CHECK(q.itemState(6, BackfillJobKind::WaveformL1Overview) == BackfillItemState::Unknown,
          "DB-A file_id=6 state forgotten after onDbClosing");

    q.onDbOpened("dbB.db", 2);

    // Phase 3: DB-B reuses the same file_ids (SQLite rowids commonly restart
    // at 1 per library). Without the purge, enqueue file_id=5 would no-op
    // because of stale Cached state from DB-A.
    q.enqueue(5, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);
    CHECK(q.itemState(5, BackfillJobKind::CoverResolveAndThumb) == BackfillItemState::Pending,
          "DB-B file_id=5 reuses the key cleanly (no DB-A collision)");
    BackfillJob j2;
    CHECK(q.dequeue(j2), "DB-B file_id=5 dequeues");
    CHECK(j2.fileId == 5, "fileId matches");
    CHECK(j2.dbGeneration == 2, "stamped with DB-B generation");

    q.shutdown();
}

// ── Test 13b: scan rejected in close→open window (Codex v8 P1 #28) ────────
static void test_enqueue_from_scan_rejected_in_close_open_window() {
    std::printf("\n[13b] enqueueFromScan rejected after onDbClosing (close→open gap)\n");
    BackfillQueue q;
    q.onDbOpened("dbA.db", 1);

    // Scan thread captured (dbA.db, 1) at scan start. Normally that snapshot
    // matches and enqueue succeeds.
    q.enqueueFromScan(100, BackfillJobKind::CoverResolveAndThumb,
                      BackfillPriority::RecentScan, "dbA.db", 1);
    auto s0 = q.snapshot();
    CHECK(s0.totalEnqueued == 1, "baseline: matching snapshot enqueue succeeds");

    // UI starts a DB switch. After onDbClosing, the snapshot guard MUST
    // reject any further (dbA.db, 1) enqueues even though onDbOpened has not
    // yet installed a new generation — otherwise jobs would survive the
    // purge and reach the new DB later.
    q.onDbClosing(1);
    q.enqueueFromScan(101, BackfillJobKind::CoverResolveAndThumb,
                      BackfillPriority::RecentScan, "dbA.db", 1);
    auto s1 = q.snapshot();
    CHECK(s1.totalEnqueued == 1, "post-onDbClosing enqueue did NOT count as success");
    CHECK(s1.scanStaleRejected >= 1, "post-onDbClosing enqueue rejected");
    CHECK(s1.byKind[static_cast<int>(BackfillJobKind::CoverResolveAndThumb)] == 0,
          "no queued entries after purge + rejected enqueue");

    // After onDbOpened, the queue is again live for the new generation only.
    q.onDbOpened("dbB.db", 2);
    q.enqueueFromScan(102, BackfillJobKind::CoverResolveAndThumb,
                      BackfillPriority::RecentScan, "dbA.db", 1);
    auto s2 = q.snapshot();
    CHECK(s2.scanStaleRejected >= 2, "stale-snapshot enqueue against new gen rejected");

    q.enqueueFromScan(103, BackfillJobKind::CoverResolveAndThumb,
                      BackfillPriority::RecentScan, "dbB.db", 2);
    auto s3 = q.snapshot();
    CHECK(s3.byKind[static_cast<int>(BackfillJobKind::CoverResolveAndThumb)] == 1,
          "new-gen matching snapshot enqueues normally");

    q.shutdown();
}

// ── Test 14: dequeue returns false promptly on shutdown (Codex v7 P2 #27) ─
static void test_shutdown_promptness_with_queued_jobs() {
    std::printf("\n[14] shutdown returns false even with queued jobs (no drain-first)\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    // Seed a non-empty queue.
    for (int i = 1; i <= 100; ++i) {
        q.enqueue(i, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::RecentScan);
    }
    auto sBefore = q.snapshot();
    CHECK(sBefore.byKind[0] == 100, "100 jobs queued before shutdown");

    auto t0 = std::chrono::steady_clock::now();
    q.shutdown();

    // Immediately after shutdown, dequeue must return false — NOT serve the
    // remaining 100 queued items first.
    BackfillJob j;
    bool got = q.dequeue(j);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    CHECK(!got, "dequeue returned false despite 100 jobs being queued");
    CHECK(elapsed < 50, "dequeue returned within 50 ms (no drain-first behavior)");
    if (elapsed >= 50) {
        std::printf("  observed %lld ms\n", (long long)elapsed);
    }
}

// ── Test 15: enqueueFromScan TOCTOU under concurrency (Codex v7 P1 #25) ───
static void test_enqueue_from_scan_no_toctou() {
    std::printf("\n[15] enqueueFromScan + concurrent onDbOpened (TOCTOU stress)\n");
    BackfillQueue q;
    q.onDbOpened("dbA.db", 100);

    constexpr int kAttempts = 5000;
    std::atomic<int> stampedOnA{0};
    std::atomic<int> stampedOnB{0};
    std::atomic<int> done{0};

    // Switcher thread: races onDbOpened between A and B.
    std::thread switcher([&]() {
        for (int i = 0; i < 500; ++i) {
            q.onDbClosing(100);
            q.onDbOpened("dbB.db", 200);
            q.onDbClosing(200);
            q.onDbOpened("dbA.db", 100);
        }
    });

    // Scan thread: always claims to be from DB-A gen 100.
    std::thread scanner([&]() {
        for (int i = 0; i < kAttempts; ++i) {
            q.enqueueFromScan(i + 1, BackfillJobKind::CoverResolveAndThumb,
                              BackfillPriority::RecentScan, "dbA.db", 100);
        }
        done.store(1);
    });

    // Dequeue thread: classifies what we drained, by generation.
    std::thread drainer([&]() {
        while (done.load() == 0 || q.snapshot().byKind[0] > 0) {
            BackfillJob j;
            // Non-blocking-ish drain: try once, sleep a bit if empty.
            auto s = q.snapshot();
            if (s.byKind[0] == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            if (q.dequeue(j)) {
                if (j.dbGeneration == 100) stampedOnA.fetch_add(1);
                else if (j.dbGeneration == 200) stampedOnB.fetch_add(1);
                q.reportOutcome(j, BackfillCompletionOutcome::Success);
            }
        }
    });

    scanner.join();
    switcher.join();
    // Give drainer time to finish, then shutdown to unstick it.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    q.shutdown();
    drainer.join();

    // Core invariant: NO job from a scanner claiming gen=100 ever ended up
    // stamped as gen=200. Either the snapshot matched (passed through,
    // stamped 100), or the snapshot mismatched (silently dropped, counted
    // in scanStaleRejected). Crossing generations is the bug.
    CHECK(stampedOnB.load() == 0,
          "no scanner job leaked into DB-B generation (TOCTOU race closed)");

    auto sFinal = q.snapshot();
    // Exact accounting is intentionally not asserted: under concurrent
    // onDbClosing(), some jobs that successfully enqueued at gen=100 get
    // legitimately vaporized by the next onDbClosing(100) call before the
    // drainer can pick them up. That is the SAME mechanism that protects
    // cross-DB isolation (Codex v7 P1 #26 fix). The two P1 invariants this
    // test actually guards — "no leak to B" + accounting >= some progress —
    // are what matter.
    std::printf("  attempts=%d stampedA=%d stampedB=%d staleRejected=%d stillQueued=%d (vaporized=%d)\n",
                kAttempts, stampedOnA.load(), stampedOnB.load(),
                sFinal.scanStaleRejected,
                sFinal.byKind[static_cast<int>(BackfillJobKind::CoverResolveAndThumb)],
                kAttempts - stampedOnA.load() - stampedOnB.load() -
                    sFinal.scanStaleRejected -
                    sFinal.byKind[static_cast<int>(BackfillJobKind::CoverResolveAndThumb)]);
    CHECK(stampedOnA.load() + sFinal.scanStaleRejected > 0,
          "scanner made forward progress under contention");
}

static void test_try_dequeue_kind_preserves_other_kinds() {
    std::printf("\n[16] tryDequeueKind preserves other job kinds\n");
    BackfillQueue q;
    q.onDbOpened("test.db", 1);

    q.enqueue(1, BackfillJobKind::CoverResolveAndThumb, BackfillPriority::SelectedFile);
    q.enqueue(2, BackfillJobKind::WaveformL1Overview, BackfillPriority::RecentScan);

    BackfillJob waveform;
    CHECK(q.tryDequeueKind(BackfillJobKind::WaveformL1Overview, waveform),
          "waveform job can be selected behind a higher-priority cover job");
    CHECK(waveform.fileId == 2, "selected waveform fileId matches");
    CHECK(waveform.kind == BackfillJobKind::WaveformL1Overview, "selected job kind is waveform");

    auto s = q.snapshot();
    CHECK(s.byKind[static_cast<int>(BackfillJobKind::CoverResolveAndThumb)] == 1,
          "cover job remains queued after waveform-only dequeue");
    CHECK(s.byKind[static_cast<int>(BackfillJobKind::WaveformL1Overview)] == 0,
          "waveform queued counter decremented");
    q.reportOutcome(waveform, BackfillCompletionOutcome::Success);

    BackfillJob cover;
    CHECK(q.tryDequeueKind(BackfillJobKind::CoverResolveAndThumb, cover),
          "preserved cover job can still be dequeued");
    CHECK(cover.fileId == 1, "preserved cover fileId matches");
    q.reportOutcome(cover, BackfillCompletionOutcome::Success);

    CHECK(!q.tryDequeueKind(BackfillJobKind::WaveformL1Overview, waveform),
          "no waveform job remains");
}

}  // namespace

int main() {
    std::printf("test_backfill_queue — slice7-A Card 1 runtime mechanics\n");
    std::printf("=========================================================\n");

    test_job_memory_budget();
    test_round_trip();
    test_dedupe_equal_priority();
    test_promote_on_higher_enqueue();
    test_priority_ordering();
    test_fifo_within_priority();
    test_shutdown_unblocks_dequeue();
    test_enqueue_from_scan_stale_drops();
    test_scan_session_id_round_trip();
    test_report_outcome_success();
    test_report_outcome_transient();
    test_report_outcome_transient_retry_budget();
    test_multi_producer_consumer();
    test_on_db_closing_purges_state();
    test_enqueue_from_scan_rejected_in_close_open_window();
    test_shutdown_promptness_with_queued_jobs();
    test_enqueue_from_scan_no_toctou();
    test_try_dequeue_kind_preserves_other_kinds();

    std::printf("\n=========================================================\n");
    std::printf("Summary: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
