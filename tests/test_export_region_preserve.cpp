// TDD: 框选一段拖入 DAW 只导出选区（Bug: 调整 Pitch 后重载同文件会清除选区，导致导出整轨）
// 本测试验证「同文件重载时是否应保留 in/out」的判定逻辑；与 AudioEngine 中保留选区的条件一致。
// Task 5: 纯逻辑回归 — region fade vs 固定 10ms 导出边沿策略、norm→帧换算与 ClampRegionFades 一致性。
#include <string>
#include <cmath>
#include <iostream>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include "src/AudioEngine.h"
#include "src/TransferEngine.h"
#include "src/WaveformRegionEditor.h"

namespace {
// 与 AudioEngine 中保留选区逻辑一致：同路径且用户曾设过选区（非默认 0~1）
constexpr float kRegionPreserveEpsilon = 0.001f;
inline bool shouldPreserveRegionOnSameFileReload(
    const std::string& oldPath, const std::string& newPath,
    float savedIn, float savedOut)
{
    return (newPath == oldPath && (savedIn > kRegionPreserveEpsilon || savedOut < (1.0f - kRegionPreserveEpsilon)));
}

// Mirrors export policy in AudioEngine::exportRegion: symmetric ~10ms only when both UI norms ~0.
inline bool exportUsesSymmetric10msDeclickOnly(float regionFadeInNorm, float regionFadeOutNorm) {
    return regionFadeInNorm <= 1e-6f && regionFadeOutNorm <= 1e-6f;
}

inline std::uint64_t regionFadeNormToFrames(float norm, std::uint64_t regionFrames) {
    return (std::uint64_t)((double)norm * (double)regionFrames + 0.5);
}

inline bool hasTransferProcessing(bool mono,
                                  int soloMode,
                                  bool reversed,
                                  float pitch,
                                  float volume,
                                  float pan,
                                  float centerSense) {
    return mono ||
           soloMode != 0 ||
           reversed ||
           std::abs(pitch) > 0.01f ||
           std::abs(volume - 1.0f) > 0.01f ||
           std::abs(pan) > 0.001f ||
           std::abs(centerSense) > 0.001f;
}

inline bool shouldUseProcessedTransferCopy(bool multiSelect,
                                           bool isCurrentFile,
                                           bool hasProcessing,
                                           bool hasRegion) {
    if (multiSelect) return hasProcessing;
    return isCurrentFile && (hasProcessing || hasRegion);
}

inline bool shouldIncludeRegionInTransfer(bool multiSelect,
                                          bool isCurrentFile,
                                          bool hasRegion) {
    return !multiSelect && isCurrentFile && hasRegion;
}

inline std::uint64_t guardedFramesRemainingAfterResamplerConsume(std::uint64_t framesRemaining,
                                                                 std::uint64_t inConsumed) {
    return (inConsumed >= framesRemaining) ? 0 : (framesRemaining - inConsumed);
}

inline std::uint64_t computeProcessedExportToRead(bool needsResampling,
                                                  std::uint64_t chunkSize,
                                                  std::uint64_t pendingInputFrames,
                                                  std::uint64_t framesRemaining,
                                                  std::uint64_t decoderFramesLeft) {
    std::uint64_t toRead = needsResampling ? (chunkSize - pendingInputFrames) : chunkSize;
    const std::uint64_t bound = needsResampling ? decoderFramesLeft : framesRemaining;
    return (toRead < bound) ? toRead : bound;
}
}

int main() {
    int failed = 0;
    // 有选区 + 同文件重载 → 应保留
    if (!shouldPreserveRegionOnSameFileReload("C:\\a.wav", "C:\\a.wav", 0.2f, 0.5f)) {
        std::cerr << "FAIL: same file, had region (0.2,0.5), should preserve\n";
        failed++;
    }
    // 有选区 + 不同文件 → 不保留
    if (shouldPreserveRegionOnSameFileReload("C:\\a.wav", "C:\\b.wav", 0.2f, 0.5f)) {
        std::cerr << "FAIL: different file, should not preserve\n";
        failed++;
    }
    // 无选区（默认 0~1）+ 同文件 → 不保留
    if (shouldPreserveRegionOnSameFileReload("C:\\a.wav", "C:\\a.wav", 0.0f, 1.0f)) {
        std::cerr << "FAIL: same file but no region (0,1), should not preserve\n";
        failed++;
    }
    // 仅入点有选区（out=1）
    if (!shouldPreserveRegionOnSameFileReload("C:\\a.wav", "C:\\a.wav", 0.3f, 1.0f)) {
        std::cerr << "FAIL: same file, in=0.3 out=1, should preserve\n";
        failed++;
    }
    // 仅出点有选区（in=0）
    if (!shouldPreserveRegionOnSameFileReload("C:\\a.wav", "C:\\a.wav", 0.0f, 0.7f)) {
        std::cerr << "FAIL: same file, in=0 out=0.7, should preserve\n";
        failed++;
    }

    // --- Task 5 policy ---
    if (!exportUsesSymmetric10msDeclickOnly(0.0f, 0.0f)) {
        std::cerr << "FAIL: (0,0) should use symmetric 10ms path\n";
        failed++;
    }
    if (!exportUsesSymmetric10msDeclickOnly(1e-7f, 0.0f)) {
        std::cerr << "FAIL: tiny norm below epsilon should still use 10ms path\n";
        failed++;
    }
    if (exportUsesSymmetric10msDeclickOnly(1e-5f, 0.0f)) {
        std::cerr << "FAIL: non-zero fade in should not use 10ms-only path\n";
        failed++;
    }
    if (exportUsesSymmetric10msDeclickOnly(0.0f, 1e-5f)) {
        std::cerr << "FAIL: non-zero fade out should not use 10ms-only path\n";
        failed++;
    }

    if (regionFadeNormToFrames(0.5f, 10000) != 5000) {
        std::cerr << "FAIL: norm to frames rounding\n";
        failed++;
    }

    // --- Transfer payload policy ---
    if (!hasTransferProcessing(false, 0, false, 2.0f, 1.0f, 0.0f, 0.0f)) {
        std::cerr << "FAIL: pitch change should count as transfer processing\n";
        failed++;
    }
    if (hasTransferProcessing(false, 0, false, 0.0f, 1.0f, 0.0f, 0.0f)) {
        std::cerr << "FAIL: neutral settings should not count as transfer processing\n";
        failed++;
    }
    if (!shouldUseProcessedTransferCopy(false, true, false, true)) {
        std::cerr << "FAIL: single current file with region should export processed copy\n";
        failed++;
    }
    if (!shouldIncludeRegionInTransfer(false, true, true)) {
        std::cerr << "FAIL: single current file should keep region in transfer\n";
        failed++;
    }
    if (!shouldUseProcessedTransferCopy(false, true, true, false)) {
        std::cerr << "FAIL: single current file with DSP should export processed copy\n";
        failed++;
    }
    if (shouldUseProcessedTransferCopy(false, false, true, false)) {
        std::cerr << "FAIL: single non-current file should stay raw even if current DSP is adjusted\n";
        failed++;
    }
    if (!shouldUseProcessedTransferCopy(true, false, true, true)) {
        std::cerr << "FAIL: multi-select with DSP should export processed copies for every file\n";
        failed++;
    }
    if (shouldIncludeRegionInTransfer(true, true, true)) {
        std::cerr << "FAIL: multi-select should not reuse current file region export\n";
        failed++;
    }
    if (shouldUseProcessedTransferCopy(true, false, false, false)) {
        std::cerr << "FAIL: multi-select without DSP should keep raw paths\n";
        failed++;
    }

    // --- Region loop policy ---
    if (ShouldRestartLoopedRegionFrames(850, 200, 1000)) {
        std::cerr << "FAIL: cursor before loop threshold should not restart\n";
        failed++;
    }
    if (!ShouldRestartLoopedRegionFrames(936, 200, 1000)) {
        std::cerr << "FAIL: cursor inside tail slack should restart loop\n";
        failed++;
    }
    if (!ShouldRestartLoopedRegionFrames(999, 200, 1000)) {
        std::cerr << "FAIL: cursor at region end should restart loop\n";
        failed++;
    }
    if (ShouldRestartLoopedRegionFrames(42, 500, 500)) {
        std::cerr << "FAIL: zero-length region should never restart loop\n";
        failed++;
    }

    // Unselected full-sound loop must wrap both matching-length files and files
    // whose decoder length is shorter than reported metadata (common across
    // formats/sample-rates). A boxed region still uses the explicit outFrame.
    if (ResolveLoopedFullSoundOutFrame(48000, 48000) != 48000) {
        std::cerr << "FAIL: matching reported/decoded lengths should keep the full-sound out frame\n";
        failed++;
    }
    if (ResolveLoopedFullSoundOutFrame(48000, 44100) != 44100) {
        std::cerr << "FAIL: shorter decoded length must cap the unselected full-sound out frame\n";
        failed++;
    }
    if (!ShouldRestartUnselectedFullSoundLoop(44080, 48000, 44100)) {
        std::cerr << "FAIL: first file with matching-enough decoded length should still wrap\n";
        failed++;
    }
    if (!ShouldRestartUnselectedFullSoundLoop(44100, 48000, 44100)) {
        std::cerr << "FAIL: switching to a shorter decoded file must still wrap at its real end\n";
        failed++;
    }
    if (ShouldRestartUnselectedFullSoundLoop(20000, 48000, 44100)) {
        std::cerr << "FAIL: mid-file cursor on a shorter decoded file should not wrap yet\n";
        failed++;
    }
    if (!ShouldRestartLoopedRegionFrames(936, 200, 1000) ||
        ShouldRestartLoopedRegionFrames(850, 200, 1000)) {
        std::cerr << "FAIL: boxed-region restart must keep using the explicit outFrame, not decoded length\n";
        failed++;
    }

    // Production path: metadata totalFrames must not be reused as decoded length.
    // A cursor at the real decoded tail of a shorter second/third file must wrap,
    // while a boxed region still ignores decoded length.
    {
        const uint64_t metadataA = 48000;
        const uint64_t decodedA = 48000;
        const uint64_t metadataB = 48000;
        const uint64_t decodedB = 44100;
        const uint64_t metadataC = 96000;
        const uint64_t decodedC = 22050;
        if (!ShouldRestartUnselectedFullSoundLoop(47950, metadataA, decodedA)) {
            std::cerr << "FAIL: first matching-length file should wrap near its decoded tail\n";
            failed++;
        }
        if (!ShouldRestartUnselectedFullSoundLoop(44100, metadataB, decodedB)) {
            std::cerr << "FAIL: switching to a shorter decoded file must wrap at decodedB, not metadataB\n";
            failed++;
        }
        if (!ShouldRestartUnselectedFullSoundLoop(22050, metadataC, decodedC)) {
            std::cerr << "FAIL: a third shorter decoded file must wrap independently of earlier files\n";
            failed++;
        }
        if (ShouldRestartUnselectedFullSoundLoop(10000, metadataC, decodedC)) {
            std::cerr << "FAIL: empty/mid-file cursor on a later file must not wrap\n";
            failed++;
        }
        if (!ShouldRestartLoopedRegionFrames(936, 200, 1000) ||
            ShouldRestartLoopedRegionFrames(850, 200, 1000)) {
            std::cerr << "FAIL: boxed selection must keep looping the original region, not decodedC\n";
            failed++;
        }
    }

    // Sync loadFile/applyLoadFileResult must not publish QueryLength as decoded
    // PCM (Greptile), but an unknown decoded length must still leave a usable
    // loop endpoint so the main feature keeps working after the user opens the
    // loop on an unselected full sound. End-to-end decision: cursor at the
    // reported tail wraps back to frame 0.
    if (ResolveDecodedPlaybackTotalFramesFromSyncLoad(48000, 0) != 0) {
        std::cerr << "FAIL: sync load without PCM must keep decoded unknown (0)\n";
        failed++;
    }
    if (ResolveDecodedPlaybackTotalFramesFromSyncLoad(48000, 44100) != 44100) {
        std::cerr << "FAIL: sync load must use actual PCM frames when they exist\n";
        failed++;
    }
    {
        // Regression: unselected loop after a sync/streaming load (decoded
        // unknown) must truly wrap back to the start, not stall.
        const uint64_t metadata = 48000;
        const uint64_t decodedUnknown = 0;
        const FullSoundLoopWrap atTail =
            DecideUnselectedFullSoundLoopWrap(47950, metadata, decodedUnknown, true, true, true);
        if (!atTail.wrap || atTail.seekFrame != 0) {
            std::cerr << "FAIL: sync load decoded-unknown must wrap the unselected loop at the reported tail\n";
            failed++;
        }
        const FullSoundLoopWrap midFile =
            DecideUnselectedFullSoundLoopWrap(20000, metadata, decodedUnknown, true, true, true);
        if (midFile.wrap) {
            std::cerr << "FAIL: mid-file cursor must not wrap yet\n";
            failed++;
        }
    }
    {
        // When a real decoded length is known and shorter than metadata
        // (truncated BWF/FACT), the loop must wrap at the real end, not the
        // inflated metadata tail.
        const uint64_t metadata = 48000;
        const uint64_t decodedReal = 44100;
        const FullSoundLoopWrap atRealTail =
            DecideUnselectedFullSoundLoopWrap(44100, metadata, decodedReal, true, true, true);
        if (!atRealTail.wrap || atRealTail.seekFrame != 0) {
            std::cerr << "FAIL: known decoded length must wrap at the real end, not metadata\n";
            failed++;
        }
    }

    // Sync load with decoded=0 keeps looping at the reported tail (covered
    // above). Boxed selection must keep looping the explicit region,
    // independent of the decoded-length policy.
    if (!ShouldRestartLoopedRegionFrames(936, 200, 1000) ||
        ShouldRestartLoopedRegionFrames(850, 200, 1000)) {
        std::cerr << "FAIL: boxed region must still use the explicit outFrame\n";
        failed++;
    }

    // A paused unselected loop must not Start() itself; wrap/seek only while playing.
    if (ShouldSeekLoopedRegionWhilePlaying(false, true)) {
        std::cerr << "FAIL: paused unselected loop must not auto-start on wrap\n";
        failed++;
    }
    if (!ShouldSeekLoopedRegionWhilePlaying(true, true)) {
        std::cerr << "FAIL: playing unselected loop must still seek on wrap\n";
        failed++;
    }
    if (ShouldSeekLoopedRegionWhilePlaying(true, false)) {
        std::cerr << "FAIL: playing cursor before wrap must not seek\n";
        failed++;
    }

    // User QA regression: some sounds had metadata longer than the real
    // playable data. The cursor can never reach the metadata tail, so the
    // only safe signal is "playback stopped while the loop was armed".
    // Exhausted (playing=false, playIntent=true) must restart from frame 0;
    // a user pause (playIntent=false) must stay stopped.
    {
        const uint64_t metadata = 48000;
        const uint64_t decodedUnknown = 0;
        const FullSoundLoopWrap exhausted =
            DecideUnselectedFullSoundLoopWrap(40000, metadata, decodedUnknown, false, true, true);
        if (!exhausted.wrap || exhausted.seekFrame != 0) {
            std::cerr << "FAIL: source-exhausted unselected loop must restart from frame 0\n";
            failed++;
        }
        const FullSoundLoopWrap userPaused =
            DecideUnselectedFullSoundLoopWrap(40000, metadata, decodedUnknown, false, false, true);
        if (userPaused.wrap) {
            std::cerr << "FAIL: user-paused loop must not auto-restart\n";
            failed++;
        }
        // Right after a fresh Start the device may not yet report playing, but
        // the pass was never observed playing: must not restart then.
        const FullSoundLoopWrap justStarted =
            DecideUnselectedFullSoundLoopWrap(10, metadata, decodedUnknown, false, true, false);
        if (justStarted.wrap) {
            std::cerr << "FAIL: not-yet-observed pass must not restart\n";
            failed++;
        }
        // metadata == real length: playing cursor at tail still wraps normally.
        const FullSoundLoopWrap playingTail =
            DecideUnselectedFullSoundLoopWrap(47950, metadata, decodedUnknown, true, true, true);
        if (!playingTail.wrap || playingTail.seekFrame != 0) {
            std::cerr << "FAIL: playing cursor at reported tail must wrap to frame 0\n";
            failed++;
        }
        const FullSoundLoopWrap playingMid =
            DecideUnselectedFullSoundLoopWrap(20000, metadata, decodedUnknown, true, true, true);
        if (playingMid.wrap) {
            std::cerr << "FAIL: mid-file playing cursor must not wrap\n";
            failed++;
        }
        // decoded known: cursor reached the real decoded end and stopped -> wrap.
        const FullSoundLoopWrap decodedExhausted =
            DecideUnselectedFullSoundLoopWrap(44100, 48000, 44100, false, true, true);
        if (!decodedExhausted.wrap) {
            std::cerr << "FAIL: stopped at the real decoded end must wrap\n";
            failed++;
        }
        // decoded known and shorter than metadata: playing at inflated metadata
        // tail is not required; wrap fires at the decoded length instead.
        const FullSoundLoopWrap decodedKnownMid =
            DecideUnselectedFullSoundLoopWrap(30000, 48000, 44100, true, true, true);
        if (decodedKnownMid.wrap) {
            std::cerr << "FAIL: playing mid-file with known decoded length must not wrap yet\n";
            failed++;
        }
    }

    // OGG user QA regression ("needs a second click"): stb_vorbis push-mode
    // reports QueryLength 0, so totalFrames stays 0 on the first pass and the
    // old pump bailed before any wrap logic. The exhaustion decision must be
    // length-agnostic: an armed, observed-playing, stopped loop restarts even
    // with totalFrames == 0 and cursor reset to 0.
    if (!ShouldRestartExhaustedUnselectedLoop(true, true, false, true, true)) {
        std::cerr << "FAIL: OGG unknown-length pass must restart when stopped with intent\n";
        failed++;
    }
    if (ShouldRestartExhaustedUnselectedLoop(true, true, false, true, false)) {
        std::cerr << "FAIL: not-yet-played pass must not restart\n";
        failed++;
    }
    if (ShouldRestartExhaustedUnselectedLoop(true, true, false, false, true)) {
        std::cerr << "FAIL: paused (no intent) must not restart\n";
        failed++;
    }
    if (ShouldRestartExhaustedUnselectedLoop(true, true, true, true, true)) {
        std::cerr << "FAIL: still-playing pass must not be treated as exhausted\n";
        failed++;
    }
    if (ShouldRestartExhaustedUnselectedLoop(false, true, false, true, true)) {
        std::cerr << "FAIL: boxed selections keep their own explicit stop path\n";
        failed++;
    }
    // Same sound with an inflated/zero length must still decide via the pure
    // wrap path: stopped + intent + observed, cursor already 0.
    {
        const FullSoundLoopWrap oggResetCursor =
            DecideUnselectedFullSoundLoopWrap(0, 0, 0, false, true, true);
        if (!oggResetCursor.wrap || oggResetCursor.seekFrame != 0) {
            std::cerr << "FAIL: cursor reset to 0 after EOF must still wrap\n";
            failed++;
        }
    }

    // Audit blocker OGG_FIRST_PASS_UNARMED_WHEN_TOTALFRAMES_ZERO: the pump
    // fallback is useless unless play()/playRegion() actually arm region
    // playback when an unselected loop is requested and the length is still
    // unknown (OGG push-mode). The arming decision must be length-agnostic.
    if (!ShouldArmRegionLoopWithoutLength(0, 1.0f, true)) {
        std::cerr << "FAIL: unselected loop request with totalFrames==0 must arm + start\n";
        failed++;
    }
    if (ShouldArmRegionLoopWithoutLength(0.2f, 0.8f, true)) {
        std::cerr << "FAIL: boxed selection must not use the unknown-length arm path\n";
        failed++;
    }
    if (ShouldArmRegionLoopWithoutLength(0.0f, 1.0f, false)) {
        std::cerr << "FAIL: plain play without loop must not arm region playback\n";
        failed++;
    }
    if (ShouldArmRegionLoopWithoutLength(0.5f, 1.0f, true)) {
        std::cerr << "FAIL: partial in-point counts as a selection, not the full sound\n";
        failed++;
    }
    if (ShouldArmRegionLoopWithoutLength(0.0f, 0.6f, true)) {
        std::cerr << "FAIL: out-point below 1 is a selection, not the full sound\n";
        failed++;
    }

    // Turning loop off must disarm looping in the same tick: no wrap state may
    // survive, while a boxed pass keeps its region playback for the remainder
    // of the current pass and an unselected pass de-arms entirely.
    {
        const LoopDisarmState boxed = DecideLoopDisarmKeepCurrentPass(true);
        if (!boxed.regionPlayback || boxed.regionLoopActive) {
            std::cerr << "FAIL: boxed loop-off must keep the current pass but never stay loop-armed\n";
            failed++;
        }
        const LoopDisarmState unselected = DecideLoopDisarmKeepCurrentPass(false);
        if (unselected.regionPlayback || unselected.regionLoopActive) {
            std::cerr << "FAIL: unselected loop-off must fully de-arm region mode\n";
            failed++;
        }
        // With loop disarmed the pump must not produce a wrap even when the
        // cursor sits at the metadata tail.
        const FullSoundLoopWrap afterDisarm =
            DecideUnselectedFullSoundLoopWrap(47950, 48000, 0, true, false, true);
        if (afterDisarm.wrap) {
            std::cerr << "FAIL: disarmed loop must not wrap at the tail\n";
            failed++;
        }
    }

    HarkenSearchRegionFadeNorms a{0.6f, 0.6f};
    HarkenSearchRegionFadeNorms c = ClampRegionFades(a, HarkenSearchFadeSide::FadeIn, 1.0f);
    if (c.fadeInNorm + c.fadeOutNorm > 1.0001f) {
        std::cerr << "FAIL: ClampRegionFades should cap sum to region length\n";
        failed++;
    }

    // --- Processed export resampler tail accounting ---
    {
        const std::uint64_t guarded = guardedFramesRemainingAfterResamplerConsume(3, 5);
        if (guarded != 0) {
            std::cerr << "FAIL: processed export tail accounting should clamp underflow to zero\n";
            failed++;
        }
        if (guardedFramesRemainingAfterResamplerConsume(7, 2) != 5) {
            std::cerr << "FAIL: processed export tail accounting should preserve normal subtraction\n";
            failed++;
        }
    }

    {
        const std::uint64_t toRead = computeProcessedExportToRead(
            true,
            4096,
            4090,
            120,
            3);
        if (toRead != 3) {
            std::cerr << "FAIL: processed export should cap decoder reads by source frames left during resampling\n";
            failed++;
        }
    }

    // --- Delivery conflict policy ---
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "auro_delivery_conflict_test";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::create_directories(tmp, ec);
    std::filesystem::path wanted = tmp / "Hit.wav";
    std::filesystem::path suffix1 = tmp / "Hit_1.wav";
    { std::ofstream out(wanted, std::ios::binary); out << "existing"; }
    { std::ofstream out(suffix1, std::ios::binary); out << "existing-1"; }

    auto createNew = TransferEngine::resolveExistingDestinationPath(
        wanted.string(), ExistingFileBehavior::AlwaysCreateNew);
    if (!createNew.shouldCopy || createNew.path != (tmp / "Hit_2.wav").string()) {
        std::cerr << "FAIL: AlwaysCreateNew should pick the next free suffix without overwrite\n";
        failed++;
    }

    auto skipExisting = TransferEngine::resolveExistingDestinationPath(
        wanted.string(), ExistingFileBehavior::SkipExisting);
    if (skipExisting.shouldCopy || !skipExisting.skipped) {
        std::cerr << "FAIL: SkipExisting should not copy over an existing file\n";
        failed++;
    }

    auto replaceExisting = TransferEngine::resolveExistingDestinationPath(
        wanted.string(), ExistingFileBehavior::ReplaceExisting);
    if (!replaceExisting.shouldCopy || !replaceExisting.wouldOverwrite || replaceExisting.path != wanted.string()) {
        std::cerr << "FAIL: ReplaceExisting should explicitly mark overwrite on the requested path\n";
        failed++;
    }

    TransferPrefs namePrefs;
    namePrefs.filenameLimit = 16;
    namePrefs.stripIllegals = true;
    std::string safeName = TransferEngine::buildSafeDestinationFileName(
        "Thunder:Metal*Impact?LongName.wav", namePrefs);
    if (safeName.size() > 16 || safeName.find(':') != std::string::npos ||
        safeName.find('*') != std::string::npos || safeName.find('?') != std::string::npos ||
        safeName.rfind(".wav") != safeName.size() - 4) {
        std::cerr << "FAIL: safe delivery filename should strip illegal chars, keep extension, and obey filenameLimit\n";
        failed++;
    }

    std::string emptyName = TransferEngine::buildSafeDestinationFileName("", namePrefs);
    if (emptyName.empty()) {
        std::cerr << "FAIL: empty delivery filename should produce a safe fallback\n";
        failed++;
    }
    std::filesystem::remove_all(tmp, ec);

    if (failed) {
        std::cerr << "Total failures: " << failed << "\n";
        return 1;
    }
    std::cout << "All export region preserve tests passed.\n";
    return 0;
}
