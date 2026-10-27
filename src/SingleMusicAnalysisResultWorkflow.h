#pragma once

#include <cstdint>
#include <string>

namespace aura::music_analysis {

struct SingleMusicBpmResult {
    bool ok = false;
    float bpm = 0.0f;
    float confidence = 0.0f;
    float beatOffset = 0.0f;
};

struct SingleMusicKeyResult {
    bool ok = false;
    std::string key;
    std::string camelot;
    float confidence = 0.0f;
};

struct SingleMusicMoodResult {
    bool ok = false;
    float valence = 0.0f;
    float arousal = 0.0f;
};

struct SingleMusicAnalysisResult {
    std::uint64_t generation = 0;
    std::int64_t fileId = -1;
    bool ok = false;
    SingleMusicBpmResult bpm;
    SingleMusicKeyResult key;
    SingleMusicMoodResult mood;
};

struct SingleMusicPendingUpdate {
    bool applyBpm = false;
    float bpm = 0.0f;
    float bpmConfidence = 0.0f;
    bool applyAiAutoSource = false;
    bool applyBeatOffset = false;
    float beatOffset = 0.0f;
    bool applyFailed = false;
    bool failed = false;
    bool applyReady = false;
    bool ready = false;
    bool resetBubbleTimer = false;
};

class SingleMusicAnalysisStatePort {
public:
    virtual ~SingleMusicAnalysisStatePort() = default;
    virtual void markAnalysisComplete() = 0;
    virtual void applyPendingUpdate(const SingleMusicPendingUpdate& update) = 0;
};

class SingleMusicAnalysisPersistencePort {
public:
    virtual ~SingleMusicAnalysisPersistencePort() = default;
    virtual void persistKey(std::int64_t fileId, const std::string& key,
                            const std::string& camelot, float confidence) = 0;
    virtual void persistMood(std::int64_t fileId, float valence, float arousal) = 0;
};

class SingleMusicAnalysisViewPort {
public:
    virtual ~SingleMusicAnalysisViewPort() = default;
    virtual void applyKeyToFirstMatchingRecord(
        std::int64_t fileId, const std::string& key,
        const std::string& camelot, float confidence) = 0;
};

bool ProcessSingleMusicAnalysisResult(
    const SingleMusicAnalysisResult& result,
    std::uint64_t currentGeneration,
    SingleMusicAnalysisStatePort& state,
    SingleMusicAnalysisPersistencePort& persistence,
    SingleMusicAnalysisViewPort& view);

}  // namespace aura::music_analysis
