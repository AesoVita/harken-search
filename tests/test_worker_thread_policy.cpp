#include <cstdio>
#include "../src/WorkerThreadPolicy.h"

static int failed = 0, passed = 0;
static void CHECK(bool c, const char* d) {
    if (c) { ++passed; std::printf("  PASS: %s\n", d); }
    else   { ++failed; std::printf("  FAIL: %s\n", d); }
}

int main() {
    using namespace aura;
    std::printf("=== WorkerThreadPolicy Tests ===\n");

    // hardware_concurrency()==0 (unknown) must fall back, never return <min.
    CHECK(ClampWaveformWorkerThreads(0) == 3, "wave hc=0 -> floor 3");
    CHECK(ClampBackfillWorkerThreads(0) == 2, "backfill hc=0 -> floor 2");

    // Small machines clamp to the floor.
    CHECK(ClampWaveformWorkerThreads(2) == 3, "wave hc=2 -> floor 3");
    CHECK(ClampBackfillWorkerThreads(2) == 2, "backfill hc=2 -> floor 2");

    // Mid machines scale.
    CHECK(ClampWaveformWorkerThreads(8) == 6, "wave hc=8 -> 6 (hc-2)");
    CHECK(ClampBackfillWorkerThreads(8) == 4, "backfill hc=8 -> 4 (hc/2)");

    // Big machines clamp to the ceiling.
    CHECK(ClampWaveformWorkerThreads(32) == 8, "wave hc=32 -> ceil 8");
    CHECK(ClampBackfillWorkerThreads(32) == 6, "backfill hc=32 -> ceil 6");

    // Scan foreground backfill uses explicit scan settings when provided, but
    // 0=auto must reuse the bounded backfill policy instead of a hardcoded pool.
    CHECK(ResolveScanBackfillWorkerThreads(0, 8) == 4, "scan backfill auto hc=8 -> 4");
    CHECK(ResolveScanBackfillWorkerThreads(-5, 8) == 4, "scan backfill negative -> auto");
    CHECK(ResolveScanBackfillWorkerThreads(99, 8) == 16, "scan backfill explicit ceil 16");
    CHECK(ResolveScanBackfillWorkerThreads(1, 8) == 1, "scan backfill explicit floor 1");

    std::printf("\n=== WorkerThreadPolicy: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}
