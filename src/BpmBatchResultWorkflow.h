#pragma once

#include <cstdint>
#include <string>

namespace aura::music_analysis {

struct BpmBatchBpmResult {
    bool ok = false;
    float bpm = 0.0f;
    float confidence = 0.0f;
};

struct BpmBatchKeyResult {
    bool ok = false;
    std::string key;
    std::string camelot;
    float confidence = 0.0f;
};

struct BpmBatchMoodResult {
    bool ok = false;
    float valence = 0.0f;
    float arousal = 0.0f;
};

struct BpmBatchResult {
    bool isP2 = false;
    std::int64_t fileId = -1;
    bool ok = false;
    BpmBatchBpmResult bpm;
    BpmBatchKeyResult key;
    BpmBatchMoodResult mood;
};

struct BpmBatchViewUpdate {
    bool applyBpm = false;
    float bpm = 0.0f;
    float bpmConfidence = 0.0f;
    bool applyBpmText = false;
    std::string bpmText;

    bool applyKey = false;
    std::string key;
    std::string camelot;
    float keyConfidence = 0.0f;

    bool applyMood = false;
    float moodValence = 0.0f;
    float moodArousal = 0.0f;
};

class BpmBatchProgressPort {
public:
    virtual ~BpmBatchProgressPort() = default;
    virtual void incrementDone() = 0;
    virtual void incrementFound() = 0;
};

class BpmBatchPersistencePort {
public:
    virtual ~BpmBatchPersistencePort() = default;
    virtual void persistBpm(std::int64_t fileId, float bpm, float confidence) = 0;
    virtual void persistKey(std::int64_t fileId, const std::string& key,
                            const std::string& camelot, float confidence) = 0;
    virtual void persistMood(std::int64_t fileId, float valence, float arousal) = 0;
};

class BpmBatchViewPort {
public:
    virtual ~BpmBatchViewPort() = default;
    virtual void applyToFirstMatchingRecord(
        std::int64_t fileId, const BpmBatchViewUpdate& update) = 0;
};

bool ProcessBpmBatchResult(const BpmBatchResult& result,
                           BpmBatchProgressPort& progress,
                           BpmBatchPersistencePort& persistence,
                           BpmBatchViewPort& view);

}  // namespace aura::music_analysis
