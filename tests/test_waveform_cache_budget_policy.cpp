#include <cstdio>
#include <cstdint>

#include "src/WaveformCachePolicy.h"

static int g_failures = 0;

static void check(bool condition, const char* description)
{
    if (condition) {
        std::printf("PASS: %s\n", description);
    } else {
        std::printf("FAIL: %s\n", description);
        ++g_failures;
    }
}

int main()
{
    using namespace WaveformCachePolicy;

    check(PersistentOverviewFramesForDuration(2.0) == kShortAudioOverviewFrames,
          "short SFX use the short overview tier");
    check(PersistentOverviewFramesForDuration(90.0) == kMediumAudioOverviewFrames,
          "medium audio uses the medium overview tier");
    check(PersistentOverviewFramesForDuration(300.0) == kLongAudioOverviewFrames,
          "long audio uses the long overview tier");
    check(PersistentOverviewFramesForDuration(900.0) == kVeryLongAudioOverviewFrames,
          "very long audio uses the floor overview tier");

    check(EstimateOverviewBytes(kShortAudioOverviewFrames, 1) ==
              static_cast<std::uint64_t>(kShortAudioOverviewFrames) * kOverviewBytesPerValue,
          "mono overview stores one uint8 magnitude display envelope");
    check(EstimateOverviewBytes(kShortAudioOverviewFrames, 2) ==
              static_cast<std::uint64_t>(kShortAudioOverviewFrames) * 2u * kOverviewBytesPerValue,
          "stereo overview doubles the uint8 display-envelope byte budget");
    check(!ShouldPersistPreviewMinEnvelope(),
          "L1 stores max envelope only; min envelope is synthesized on read");

    const std::uint64_t millionMonoShort =
        EstimateOverviewBytesForDuration(15.0, 1) * 1000000ull;
    check(millionMonoShort <= kWaveformCacheTargetBytes,
          "1M mono short-SFX overview cache stays under the 512MB L1 target");

    const std::uint64_t millionStereoShort =
        EstimateOverviewBytesForDuration(15.0, 2) * 1000000ull;
    check(millionStereoShort <= kWaveformCacheHardLimitBytes,
          "1M stereo short-SFX overview cache stays under the 1GB L1 hard limit");

    check(ChooseCompactionTierFrames(15.0, kWaveformCacheTargetBytes + 1, kWaveformCacheTargetBytes) <=
              kLongAudioOverviewFrames,
          "budget pressure compacts already-written high-tier rows");
    check(!ShouldPauseNewOverviewWrites(kWaveformCacheHardLimitBytes - 1, kWaveformCacheHardLimitBytes),
          "writes continue below hard limit");
    check(ShouldPauseNewOverviewWrites(kWaveformCacheHardLimitBytes + 1, kWaveformCacheHardLimitBytes),
          "writes pause above hard limit");

    auto mainDetail = PersistentMainDetailForDuration(30.0);
    check(mainDetail.persistMainDetail && mainDetail.targetPeaks == kShortMainDetailPeaks,
          "short main detail persists at 3000 peaks");
    mainDetail = PersistentMainDetailForDuration(90.0);
    check(mainDetail.persistMainDetail && mainDetail.targetPeaks == kMediumMainDetailPeaks,
          "medium main detail persists at 2000 peaks");
    mainDetail = PersistentMainDetailForDuration(180.0);
    check(mainDetail.persistMainDetail && mainDetail.targetPeaks == kLongMainDetailPeaks,
          "long main detail persists at 1000 peaks");
    mainDetail = PersistentMainDetailForDuration(301.0);
    check(!mainDetail.persistMainDetail && mainDetail.targetPeaks == 0,
          "over 5 minutes persists mini only");
    check(!ShouldPersistGeneratedMainDetail(10.0, kZoomOnlyDetailPeaks, 1),
          "12000-peak zoom detail is never persisted for short files");
    check(ShouldPersistGeneratedMainDetail(60.0, kMediumMainDetailPeaks * 2u, 2),
          "stereo medium detail at the bucket size may persist");

    if (g_failures == 0) {
        std::printf("PASS\n");
        return 0;
    }
    std::printf("FAIL: %d assertion(s)\n", g_failures);
    return 1;
}
