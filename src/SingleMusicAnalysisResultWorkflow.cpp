#include "SingleMusicAnalysisResultWorkflow.h"

namespace aura::music_analysis {

bool ProcessSingleMusicAnalysisResult(
    const SingleMusicAnalysisResult& result,
    std::uint64_t currentGeneration,
    SingleMusicAnalysisStatePort& state,
    SingleMusicAnalysisPersistencePort& persistence,
    SingleMusicAnalysisViewPort& view) {
    if (result.generation != currentGeneration) return false;

    state.markAnalysisComplete();
    if (result.fileId >= 0 && result.key.ok && !result.key.key.empty()) {
        persistence.persistKey(result.fileId, result.key.key,
                               result.key.camelot, result.key.confidence);
        view.applyKeyToFirstMatchingRecord(result.fileId, result.key.key,
                                           result.key.camelot,
                                           result.key.confidence);
    }
    if (result.fileId >= 0 && result.mood.ok) {
        persistence.persistMood(result.fileId, result.mood.valence,
                                result.mood.arousal);
    }

    SingleMusicPendingUpdate update;
    update.applyBpm = true;
    update.applyReady = true;
    update.ready = true;
    update.resetBubbleTimer = true;
    if (result.bpm.ok) {
        update.bpm = result.bpm.bpm;
        update.bpmConfidence = result.bpm.confidence;
        update.applyAiAutoSource = true;
        update.applyBeatOffset = true;
        update.beatOffset = result.bpm.beatOffset;
    } else {
        update.applyFailed = true;
        update.failed = true;
    }
    state.applyPendingUpdate(update);
    return true;
}

}  // namespace aura::music_analysis
