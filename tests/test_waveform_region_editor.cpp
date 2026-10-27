#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "src/WaveformRegionEditor.h"

namespace {

std::string ReadRepoFile(const char* relativePath)
{
    std::filesystem::path here(__FILE__);
    std::filesystem::path root = here.parent_path().parent_path();
    std::ifstream in(root / relativePath, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string ExtractFunctionBody(const std::string& source, const std::string& signature)
{
    const size_t sigPos = source.find(signature);
    if (sigPos == std::string::npos) return {};
    const size_t open = source.find('{', sigPos);
    if (open == std::string::npos) return {};

    int depth = 0;
    for (size_t i = open; i < source.size(); ++i) {
        if (source[i] == '{') {
            ++depth;
        } else if (source[i] == '}') {
            --depth;
            if (depth == 0) {
                return source.substr(open, i - open + 1);
            }
        }
    }
    return {};
}

bool ContainsInOrder(const std::string& text, const std::string& first, const std::string& second)
{
    const size_t a = text.find(first);
    if (a == std::string::npos) return false;
    const size_t b = text.find(second, a + first.size());
    return b != std::string::npos;
}

} // namespace

int main()
{
    int failed = 0;

    // fade clamp: when sum exceeds cap, only clamp the passive side
    {
        HarkenSearchRegionFadeNorms fades{0.40f, 0.40f};
        auto clamped = ClampRegionFades(fades, HarkenSearchFadeSide::FadeIn, 0.60f);
        if (!(clamped.fadeInNorm == 0.40f && clamped.fadeOutNorm <= 0.20f + 0.0001f))
            failed++;
    }

    // auto-zoom: only when current view is wider than needed (low zoom)
    {
        HarkenSearchTrimRefineWindow current{1.0f, 0.5f};
        auto next = ComputeTrimRefineWindow(0.20f, 0.30f, current);
        if (!(next.zoom > 1.0f))
            failed++;
    }

    // auto-zoom: already zoomed in enough -> unchanged
    {
        HarkenSearchTrimRefineWindow current{8.0f, 0.25f};
        auto next = ComputeTrimRefineWindow(0.20f, 0.30f, current);
        if (!(std::fabs(next.zoom - current.zoom) < 0.0001f))
            failed++;
    }

    // zoom back to global should exit refine (caller keeps region)
    {
        if (!ShouldExitTrimRefineForGlobalView(1.0f))
            failed++;
    }

    // overlapping edge hit zones should choose the closer edge
    {
        if (PickRegionEdgeTarget(true, true, 104.0f, 100.0f, 110.0f) != HarkenSearchRegionEdgeTarget::In)
            failed++;
        if (PickRegionEdgeTarget(true, true, 108.0f, 100.0f, 110.0f) != HarkenSearchRegionEdgeTarget::Out)
            failed++;
    }

    // region fade gain should ramp inside fades and stay full in the middle
    {
        float gStart = ComputeRegionFadeGain(0.20f, 0.20f, 0.80f, 0.25f, 0.25f);
        float gMid = ComputeRegionFadeGain(0.50f, 0.20f, 0.80f, 0.25f, 0.25f);
        float gTail = ComputeRegionFadeGain(0.78f, 0.20f, 0.80f, 0.25f, 0.25f);
        if (!(gStart < 0.001f))
            failed++;
        if (!(gMid > 0.99f))
            failed++;
        if (!(gTail < 0.25f))
            failed++;
    }

    // anchored fade drag should preserve the existing handle offset when drag starts
    {
        HarkenSearchRegionFadeNorms start{0.25f, 0.10f};
        auto dragged = ComputeAnchoredFadeDrag(0.20f, 0.24f, 0.20f, 0.80f, start, HarkenSearchFadeSide::FadeIn);
        const float expected = (0.20f + 0.25f * 0.60f + 0.04f - 0.20f) / 0.60f;
        if (!(std::fabs(dragged.fadeInNorm - expected) < 0.0001f))
            failed++;
        if (!(std::fabs(dragged.fadeOutNorm - start.fadeOutNorm) < 0.0001f))
            failed++;
    }

    // anchored fade drag should preserve the edited side and clamp the passive side when total fade exceeds 100%
    {
        HarkenSearchRegionFadeNorms start{0.35f, 0.45f};
        auto dragged = ComputeAnchoredFadeDrag(0.50f, 0.80f, 0.20f, 0.80f, start, HarkenSearchFadeSide::FadeIn);
        if (!(std::fabs(dragged.fadeInNorm - 0.85f) < 0.0001f))
            failed++;
        if (!(std::fabs(dragged.fadeOutNorm - 0.15f) < 0.0001f))
            failed++;
    }

    // fade handle crowded detection still works; ShouldAllowFadeHandleEdit now
    // always returns true — priority disambiguation moved to ResolveFadeInHitVsInEdge
    // / ResolveFadeOutHitVsOutEdge (see commit 9254995, zone-based IN/OUT split).
    {
        if (!IsFadeHandleCrowded(100.0f, 114.0f, 28.0f))
            failed++;
        // All combos must return true (unconditional, parameters ignored)
        if (!ShouldAllowFadeHandleEdit(false, false, 100.0f, 114.0f, 28.0f))
            failed++;
        if (!ShouldAllowFadeHandleEdit(true, false, 100.0f, 114.0f, 28.0f))
            failed++;
        if (!ShouldAllowFadeHandleEdit(false, true, 100.0f, 114.0f, 28.0f))
            failed++;
        if (!ShouldAllowFadeHandleEdit(false, false, 100.0f, 140.0f, 28.0f))
            failed++;
    }

    // Fade handles own the entire upper half of the edge band. A click just
    // above the midpoint must start fade scaling, not move the IN/OUT endpoint.
    {
        if (!ShouldStartFadeDragFromEdgeBand(49.0f, 0.0f, 100.0f))
            failed++;
        if (ShouldStartFadeDragFromEdgeBand(51.0f, 0.0f, 100.0f))
            failed++;
    }

    // time ruler labels should drop density when width is tight
    {
        if (ComputeWaveformTimeLabelCount(320.0f, 90.0f, false) != 3)
            failed++;
        if (ComputeWaveformTimeLabelCount(1200.0f, 90.0f, false) != 10)
            failed++;
        if (ComputeWaveformTimeLabelCount(320.0f, 90.0f, true) != 3)
            failed++;
    }

    // Region drag should commit even if the exact mouse-release edge was lost.
    {
        if (!ShouldCommitRegionDragPlayback(true, false, false))
            failed++;
        if (!ShouldCommitRegionDragPlayback(true, false, true))
            failed++;
        if (ShouldCommitRegionDragPlayback(true, true, false))
            failed++;
        if (ShouldCommitRegionDragPlayback(false, false, true))
            failed++;
    }

    // Source contract: paused -> box-selected region must not restart silently.
    // pause() leaves a fade-out envelope / stop_time on the sound; playRegion()
    // must capture the pre-start playing state and explicitly re-arm a short
    // fade-in just like normal play(), otherwise the region can start at volume 0.
    {
        const std::string audio = ReadRepoFile("src/AudioEngine.cpp");
        const std::string body = ExtractFunctionBody(audio, "void AudioEngine::playRegion(");
        if (body.empty()) {
            std::fprintf(stderr, "FAIL: AudioEngine::playRegion source body not found\n");
            failed++;
        } else {
            if (!ContainsInOrder(body,
                    "const bool soundWasPlaying",
                    "ma_sound_set_stop_time_in_pcm_frames")) {
                std::fprintf(stderr,
                    "FAIL: playRegion must capture soundWasPlaying before changing stop_time\n");
                failed++;
            }
            if (body.find("ma_sound_set_fade_in_milliseconds") == std::string::npos) {
                std::fprintf(stderr,
                    "FAIL: playRegion must re-arm fade-in after paused fade-out before starting region\n");
                failed++;
            }
            if (!ContainsInOrder(body,
                    "const bool soundWasPlaying",
                    "ma_sound_reset_stop_time_and_fade(m_sound)")) {
                std::fprintf(stderr,
                    "FAIL: playRegion must clear pending pause fade/stop_time before region start\n");
                failed++;
            }
        }
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
