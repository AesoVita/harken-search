#include "BpmBatchResultWorkflow.h"

#include <cstdio>

namespace aura::music_analysis {

bool ProcessBpmBatchResult(const BpmBatchResult& result,
                           BpmBatchProgressPort& progress,
                           BpmBatchPersistencePort& persistence,
                           BpmBatchViewPort& view) {
    if (!result.isP2 || result.fileId < 0) return false;

    progress.incrementDone();
    BpmBatchViewUpdate update;

    if (!result.ok) {
        persistence.persistBpm(result.fileId, -1.0f, 0.0f);
        update.applyBpm = true;
        update.bpm = -1.0f;
        update.bpmConfidence = 0.0f;
        view.applyToFirstMatchingRecord(result.fileId, update);
        return true;
    }

    if (result.bpm.ok && result.bpm.bpm > 40.0f && result.bpm.bpm < 240.0f) {
        persistence.persistBpm(result.fileId, result.bpm.bpm,
                               result.bpm.confidence);
        progress.incrementFound();
        update.applyBpm = true;
        update.bpm = result.bpm.bpm;
        update.bpmConfidence = result.bpm.confidence;
        update.applyBpmText = true;
        char bpmText[32];
        std::snprintf(bpmText, sizeof(bpmText), "%.1f",
                      static_cast<double>(result.bpm.bpm));
        update.bpmText = bpmText;
    }
    if (result.key.ok && !result.key.key.empty()) {
        persistence.persistKey(result.fileId, result.key.key,
                               result.key.camelot, result.key.confidence);
        update.applyKey = true;
        update.key = result.key.key;
        update.camelot = result.key.camelot;
        update.keyConfidence = result.key.confidence;
    }
    if (result.mood.ok) {
        persistence.persistMood(result.fileId, result.mood.valence,
                                result.mood.arousal);
        update.applyMood = true;
        update.moodValence = result.mood.valence;
        update.moodArousal = result.mood.arousal;
    }

    view.applyToFirstMatchingRecord(result.fileId, update);
    return true;
}

}  // namespace aura::music_analysis
