// Tests for WaveformCachePolicy::ShouldPersistScanMini and the
// kMiniCachePersistenceMinSeconds production constant.
//
// Layer 1 (unit): boundary semantics for the guard function.
// Layer 6 (semantic flag): production threshold pinned at 0.0 — every audio
//                          file gets its mini overview persisted during scan.
//
// History: the original optimization (commit 5e17cdbc, threshold 40.0s) was
// reverted on 2026-06-15 user feedback because the perceived "first paint"
// gap between cached and uncached rows during list scrolling stayed obvious
// even though short-file mini decode is microsecond-fast in isolation.
//
// See SOUL.md / CLAUDE.md "行为标志语义测试" — this file guards against
// silent regressions where a debugging session changes the threshold and
// commits without realizing the production semantic intent.

#include "WaveformCachePolicy.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& description)
{
    if (condition) {
        std::printf("PASS: %s\n", description.c_str());
    } else {
        std::printf("FAIL: %s\n", description.c_str());
        ++g_failures;
    }
}

} // namespace

int main()
{
    using namespace WaveformCachePolicy;

    // ── Layer 1: ShouldPersistScanMini boundary semantics ────────────────
    // With the production threshold at 0.0, every duration must yield true.
    // We keep negative / zero / sub-second / boundary / long cases so that
    // any future re-introduction of a positive threshold breaks the test
    // suite loudly instead of silently masking short files again.
    Check(ShouldPersistScanMini(0.0) == true,
          "duration unknown (0.0) -> conservatively cache");
    Check(ShouldPersistScanMini(-1.0) == true,
          "duration negative -> conservatively cache (never silently drop)");
    Check(ShouldPersistScanMini(0.5) == true,
          "0.5s short file -> cache (post-2026-06-15: no short-file skip)");
    Check(ShouldPersistScanMini(20.0) == true,
          "20s short file -> cache (post-2026-06-15: no short-file skip)");
    Check(ShouldPersistScanMini(39.99) == true,
          "39.99s file -> cache (post-2026-06-15: no short-file skip)");
    Check(ShouldPersistScanMini(40.0) == true,
          "40.0s -> cache");
    Check(ShouldPersistScanMini(40.01) == true,
          "40.01s -> cache");
    Check(ShouldPersistScanMini(60.0) == true,
          "60s -> cache");
    Check(ShouldPersistScanMini(3600.0) == true,
          "1h long file -> cache");

    // ── Layer 6: SEMANTIC flag pinning ───────────────────────────────────
    // The constant value is the product contract: changing it without
    // updating this assertion silently drops short files from the persisted
    // mini cache and reintroduces the perceived first-paint regression that
    // user-tested feedback flagged on 2026-06-15.
    Check(kMiniCachePersistenceMinSeconds == 0.0,
          "SEMANTIC: production threshold for mini cache persistence must be 0.0s "
          "(every scanned audio file must persist its mini overview)");

    if (g_failures > 0) {
        std::printf("\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll waveform cache policy tests passed.\n");
    return 0;
}
