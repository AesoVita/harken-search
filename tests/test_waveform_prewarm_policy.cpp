#include "WaveformPrewarmPolicy.h"

#include <iostream>

namespace {

int g_failed = 0;

void Expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << "\n";
        ++g_failed;
    }
}

void TestPrewarmUsesCoarseDetailBudget()
{
    Expect(HarkenSearch::WaveformPrewarm::kDetailPrewarmPeaks == 3000,
           "background detail prewarm should use the short-file 3000-peak cache");
    Expect(HarkenSearch::WaveformPrewarm::kDetailPrewarmPeaks < 12000,
           "background detail prewarm should stay below the main waveform detail target");
}

void TestExistingDetailCacheSkipsLazyPrewarmUpgrade()
{
    Expect(!HarkenSearch::WaveformPrewarm::HasUsableDetailCache(0),
           "empty detail cache should still allow prewarm");
    Expect(HarkenSearch::WaveformPrewarm::HasUsableDetailCache(1),
           "any existing detail cache should block lazy prewarm upgrade");
    Expect(HarkenSearch::WaveformPrewarm::HasUsableDetailCache(3999),
           "partial/coarse detail cache should block lazy prewarm upgrade");
}

} // namespace

int main()
{
    TestPrewarmUsesCoarseDetailBudget();
    TestExistingDetailCacheSkipsLazyPrewarmUpgrade();

    if (g_failed != 0)
    {
        std::cerr << "FAIL: " << g_failed << " WaveformPrewarmPolicy checks failed\n";
        return 1;
    }

    std::cout << "PASS: WaveformPrewarmPolicy\n";
    return 0;
}
