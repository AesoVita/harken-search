// test_memory_budget.cpp — Unit tests for MemoryBudget (F-020)
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "../src/MemoryBudget.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

using CT = MemoryBudget::CacheType;

static constexpr uint64_t MB = 1024ULL * 1024;

int main() {
    std::printf("=== MemoryBudget Tests ===\n");

    // ── Default construction ──
    std::printf("\n--- Default construction ---\n");
    {
        MemoryBudget mb;
        auto cfg = mb.getBudgetConfig();
        CHECK(cfg.metadataBudget == 200 * MB, "default metadataBudget = 200 MB");
        CHECK(cfg.waveformBudget == 150 * MB, "default waveformBudget = 150 MB");
        CHECK(cfg.coverBudget == 250 * MB, "default coverBudget = 250 MB");
        CHECK(cfg.totalBudget == 600 * MB, "default totalBudget = 600 MB");
        CHECK(mb.getTotalUsage() == 0, "default totalUsage = 0");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 0, "default metadata usage = 0");
        CHECK(mb.getCurrentUsage(CT::Waveform) == 0, "default waveform usage = 0");
        CHECK(mb.getCurrentUsage(CT::Cover) == 0, "default cover usage = 0");
    }

    // ── setBudgetConfig / getBudgetConfig ──
    std::printf("\n--- setBudgetConfig / getBudgetConfig ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 50 * MB;
        cfg.waveformBudget = 30 * MB;
        cfg.coverBudget = 40 * MB;
        cfg.totalBudget = 100 * MB;
        mb.setBudgetConfig(cfg);

        auto got = mb.getBudgetConfig();
        CHECK(got.metadataBudget == 50 * MB, "set metadataBudget roundtrip");
        CHECK(got.waveformBudget == 30 * MB, "set waveformBudget roundtrip");
        CHECK(got.coverBudget == 40 * MB, "set coverBudget roundtrip");
        CHECK(got.totalBudget == 100 * MB, "set totalBudget roundtrip");
    }

    // ── registerEntry: normal ──
    std::printf("\n--- registerEntry normal ---\n");
    {
        MemoryBudget mb;
        bool ok = mb.registerEntry(CT::Metadata, "file1", 1024);
        CHECK(ok, "register metadata entry succeeds");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 1024, "metadata usage = 1024");
        CHECK(mb.getTotalUsage() == 1024, "total usage = 1024");

        ok = mb.registerEntry(CT::Waveform, "wave1", 2048);
        CHECK(ok, "register waveform entry succeeds");
        CHECK(mb.getCurrentUsage(CT::Waveform) == 2048, "waveform usage = 2048");
        CHECK(mb.getTotalUsage() == 1024 + 2048, "total usage = 3072");

        ok = mb.registerEntry(CT::Cover, "cover1", 4096);
        CHECK(ok, "register cover entry succeeds");
        CHECK(mb.getCurrentUsage(CT::Cover) == 4096, "cover usage = 4096");
        CHECK(mb.getTotalUsage() == 1024 + 2048 + 4096, "total = 7168");
    }

    // ── registerEntry: reject empty key / zero size ──
    std::printf("\n--- registerEntry edge cases ---\n");
    {
        MemoryBudget mb;
        bool ok = mb.registerEntry(CT::Metadata, "", 1024);
        CHECK(!ok, "empty key rejected");
        CHECK(mb.getTotalUsage() == 0, "no usage after empty key reject");

        ok = mb.registerEntry(CT::Metadata, "key1", 0);
        CHECK(!ok, "zero size rejected");
        CHECK(mb.getTotalUsage() == 0, "no usage after zero size reject");
    }

    // ── registerEntry: duplicate key updates existing ──
    std::printf("\n--- registerEntry duplicate key ---\n");
    {
        MemoryBudget mb;
        mb.registerEntry(CT::Metadata, "dup", 1000);
        CHECK(mb.getCurrentUsage(CT::Metadata) == 1000, "first register: 1000");

        bool ok = mb.registerEntry(CT::Metadata, "dup", 2000);
        CHECK(ok, "duplicate key register succeeds");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 2000, "duplicate replaces: 2000");

        auto stats = mb.getStatistics();
        CHECK(stats.metadataCount == 1, "still 1 entry after duplicate");
    }

    // ── unregisterEntry ──
    std::printf("\n--- unregisterEntry ---\n");
    {
        MemoryBudget mb;
        mb.registerEntry(CT::Cover, "c1", 5000);
        mb.registerEntry(CT::Cover, "c2", 3000);
        CHECK(mb.getCurrentUsage(CT::Cover) == 8000, "before unregister: 8000");

        mb.unregisterEntry(CT::Cover, "c1");
        CHECK(mb.getCurrentUsage(CT::Cover) == 3000, "after unregister c1: 3000");
        CHECK(mb.getTotalUsage() == 3000, "total after unregister: 3000");

        mb.unregisterEntry(CT::Cover, "nonexistent");
        CHECK(mb.getCurrentUsage(CT::Cover) == 3000, "unregister nonexistent: no change");
    }

    // ── canAdd ──
    std::printf("\n--- canAdd ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 1000;
        cfg.waveformBudget = 1000;
        cfg.coverBudget = 1000;
        cfg.totalBudget = 2000;
        mb.setBudgetConfig(cfg);

        CHECK(mb.canAdd(CT::Metadata, 500), "canAdd 500 to empty 1000 budget");
        CHECK(mb.canAdd(CT::Metadata, 1000), "canAdd 1000 exactly at budget");
        CHECK(!mb.canAdd(CT::Metadata, 1001), "cannot add 1001 to 1000 budget");

        mb.registerEntry(CT::Metadata, "fill", 800);
        CHECK(mb.canAdd(CT::Metadata, 200), "canAdd 200 with 800 used of 1000");
        CHECK(!mb.canAdd(CT::Metadata, 201), "cannot add 201 with 800 used of 1000");
    }

    // ── setCurrentFrame + updateUsage ──
    std::printf("\n--- setCurrentFrame / updateUsage ---\n");
    {
        MemoryBudget mb;
        mb.setCurrentFrame(100);
        mb.registerEntry(CT::Waveform, "w1", 1024);

        mb.setCurrentFrame(200);
        mb.registerEntry(CT::Waveform, "w2", 1024);

        mb.setCurrentFrame(300);
        mb.updateUsage(CT::Waveform, "w1");

        // w1 was updated at frame 300, w2 last at frame 200
        // evictLRU should evict w2 first (older)
        auto evicted = mb.evictLRU(CT::Waveform, 1024);
        CHECK(evicted.size() == 1, "evicted 1 entry");
        if (!evicted.empty()) {
            CHECK(evicted[0].key == "w2", "evicted w2 (older, frame 200) not w1 (frame 300)");
        }
    }

    // ── evictLRU: ordering ──
    std::printf("\n--- evictLRU ordering ---\n");
    {
        MemoryBudget mb;
        mb.setCurrentFrame(10);
        mb.registerEntry(CT::Metadata, "oldest", 1000);
        mb.setCurrentFrame(20);
        mb.registerEntry(CT::Metadata, "middle", 1000);
        mb.setCurrentFrame(30);
        mb.registerEntry(CT::Metadata, "newest", 1000);

        auto evicted = mb.evictLRU(CT::Metadata, 1500);
        CHECK(evicted.size() == 2, "evict 2 entries to free 1500 bytes");
        if (evicted.size() >= 2) {
            CHECK(evicted[0].key == "oldest", "first evicted = oldest");
            CHECK(evicted[1].key == "middle", "second evicted = middle");
        }
        CHECK(mb.getCurrentUsage(CT::Metadata) == 1000, "only newest remains: 1000");
    }

    // ── Per-type budget eviction on register ──
    std::printf("\n--- Per-type budget eviction on register ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 2000;
        cfg.waveformBudget = 2000;
        cfg.coverBudget = 2000;
        cfg.totalBudget = 10000;
        mb.setBudgetConfig(cfg);

        mb.setCurrentFrame(1);
        mb.registerEntry(CT::Metadata, "m1", 1000);
        mb.setCurrentFrame(2);
        mb.registerEntry(CT::Metadata, "m2", 800);

        // Budget is 2000, used is 1800. Adding 500 would be 2300 > 2000.
        // Should auto-evict m1 (oldest) to make room.
        mb.setCurrentFrame(3);
        bool ok = mb.registerEntry(CT::Metadata, "m3", 500);
        CHECK(ok, "register triggers per-type eviction");
        // m1 evicted (1000), so now m2(800) + m3(500) = 1300
        CHECK(mb.getCurrentUsage(CT::Metadata) == 1300, "after eviction: 1300");
    }

    // ── Total budget cross-type eviction ──
    std::printf("\n--- Total budget cross-type eviction ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 5000;
        cfg.waveformBudget = 5000;
        cfg.coverBudget = 5000;
        cfg.totalBudget = 3000;
        mb.setBudgetConfig(cfg);

        mb.setCurrentFrame(1);
        mb.registerEntry(CT::Waveform, "w1", 1000);
        mb.setCurrentFrame(2);
        mb.registerEntry(CT::Cover, "c1", 1000);

        // Total = 2000. Adding 1500 metadata would be 3500 > totalBudget(3000).
        // Cross-type eviction order: Waveform first, then Cover, then Metadata.
        mb.setCurrentFrame(3);
        bool ok = mb.registerEntry(CT::Metadata, "m1", 1500);
        CHECK(ok, "cross-type eviction succeeds");
        CHECK(mb.getTotalUsage() <= 3000, "total within budget after cross-type eviction");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 1500, "metadata registered: 1500");
    }

    // ── Cross-type eviction order: Waveform → Cover → Metadata ──
    std::printf("\n--- Cross-type eviction order ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 5000;
        cfg.waveformBudget = 5000;
        cfg.coverBudget = 5000;
        cfg.totalBudget = 2000;
        mb.setBudgetConfig(cfg);

        mb.setCurrentFrame(1);
        mb.registerEntry(CT::Metadata, "m1", 500);
        mb.setCurrentFrame(2);
        mb.registerEntry(CT::Waveform, "w1", 500);
        mb.setCurrentFrame(3);
        mb.registerEntry(CT::Cover, "c1", 500);
        // total = 1500

        // Adding 1000 would need 2500 > 2000, must evict 500.
        // Order: Waveform first, so w1 should be evicted.
        mb.setCurrentFrame(4);
        bool ok = mb.registerEntry(CT::Metadata, "m2", 1000);
        CHECK(ok, "cross-type eviction order works");

        auto pending = mb.getAndClearPendingEvicted();
        bool waveformEvicted = false;
        for (auto& [ct, entry] : pending) {
            if (ct == CT::Waveform && entry.key == "w1") waveformEvicted = true;
        }
        CHECK(waveformEvicted, "waveform evicted first in cross-type order");
    }

    // ── getAndClearPendingEvicted ──
    std::printf("\n--- getAndClearPendingEvicted ---\n");
    {
        MemoryBudget mb;
        auto empty = mb.getAndClearPendingEvicted();
        CHECK(empty.empty(), "initially no pending evicted");

        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 5000;
        cfg.waveformBudget = 5000;
        cfg.coverBudget = 5000;
        cfg.totalBudget = 1500;
        mb.setBudgetConfig(cfg);

        mb.setCurrentFrame(1);
        mb.registerEntry(CT::Waveform, "w1", 1000);
        mb.setCurrentFrame(2);
        mb.registerEntry(CT::Cover, "c1", 1000);
        // total=2000 > 1500 → cross-type eviction during c1 register

        auto pending = mb.getAndClearPendingEvicted();
        auto secondCall = mb.getAndClearPendingEvicted();
        CHECK(secondCall.empty(), "second call returns empty (cleared)");
    }

    // ── getStatistics ──
    std::printf("\n--- getStatistics ---\n");
    {
        MemoryBudget mb;
        mb.registerEntry(CT::Metadata, "m1", 100);
        mb.registerEntry(CT::Metadata, "m2", 200);
        mb.registerEntry(CT::Waveform, "w1", 300);
        mb.registerEntry(CT::Cover, "c1", 400);
        mb.registerEntry(CT::Cover, "c2", 500);
        mb.registerEntry(CT::Cover, "c3", 600);

        auto stats = mb.getStatistics();
        CHECK(stats.metadataUsage == 300, "stats metadataUsage = 300");
        CHECK(stats.metadataCount == 2, "stats metadataCount = 2");
        CHECK(stats.waveformUsage == 300, "stats waveformUsage = 300");
        CHECK(stats.waveformCount == 1, "stats waveformCount = 1");
        CHECK(stats.coverUsage == 1500, "stats coverUsage = 1500");
        CHECK(stats.coverCount == 3, "stats coverCount = 3");
        CHECK(stats.totalUsage == 2100, "stats totalUsage = 2100");
    }

    // ── evictLRU: not enough to evict ──
    std::printf("\n--- evictLRU not enough ---\n");
    {
        MemoryBudget mb;
        mb.registerEntry(CT::Metadata, "m1", 500);
        auto evicted = mb.evictLRU(CT::Metadata, 1000);
        CHECK(evicted.size() == 1, "evicts what it can");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 0, "all metadata evicted");
    }

    // ── evictLRU: empty cache ──
    std::printf("\n--- evictLRU empty cache ---\n");
    {
        MemoryBudget mb;
        auto evicted = mb.evictLRU(CT::Waveform, 1000);
        CHECK(evicted.empty(), "evict from empty cache returns nothing");
    }

    // ── Register with userData ──
    std::printf("\n--- registerEntry userData ---\n");
    {
        MemoryBudget mb;
        int dummy = 42;
        bool ok = mb.registerEntry(CT::Cover, "tex1", 2048, &dummy);
        CHECK(ok, "register with userData succeeds");

        auto evicted = mb.evictLRU(CT::Cover, 2048);
        CHECK(evicted.size() == 1, "evicted 1 entry");
        if (!evicted.empty()) {
            CHECK(evicted[0].userData == &dummy, "evicted entry preserves userData");
        }
    }

    // ── Register fails when single entry exceeds budget ──
    std::printf("\n--- Register exceeds budget ---\n");
    {
        MemoryBudget mb;
        MemoryBudget::BudgetConfig cfg;
        cfg.metadataBudget = 1000;
        cfg.waveformBudget = 1000;
        cfg.coverBudget = 1000;
        cfg.totalBudget = 1000;
        mb.setBudgetConfig(cfg);

        bool ok = mb.registerEntry(CT::Metadata, "huge", 2000);
        CHECK(!ok, "entry larger than budget rejected");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 0, "no usage after reject");
    }

    // ── Multiple types: independent tracking ──
    std::printf("\n--- Independent type tracking ---\n");
    {
        MemoryBudget mb;
        mb.registerEntry(CT::Metadata, "same_key", 100);
        mb.registerEntry(CT::Waveform, "same_key", 200);
        mb.registerEntry(CT::Cover, "same_key", 300);

        CHECK(mb.getCurrentUsage(CT::Metadata) == 100, "metadata tracks independently");
        CHECK(mb.getCurrentUsage(CT::Waveform) == 200, "waveform tracks independently");
        CHECK(mb.getCurrentUsage(CT::Cover) == 300, "cover tracks independently");
        CHECK(mb.getTotalUsage() == 600, "total = sum of all types");

        mb.unregisterEntry(CT::Waveform, "same_key");
        CHECK(mb.getCurrentUsage(CT::Waveform) == 0, "waveform removed");
        CHECK(mb.getCurrentUsage(CT::Metadata) == 100, "metadata unaffected");
        CHECK(mb.getCurrentUsage(CT::Cover) == 300, "cover unaffected");
    }

    std::printf("\n=== MemoryBudget: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
