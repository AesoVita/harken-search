#pragma once

#include "AIEngine.h"

#include <cstdint>
#include <string>
#include <vector>

// @stage7-contract
// API contract: VoiceAssetOrganizer performs synchronous, deterministic
// transformations over caller-owned filename/transcript values and returns
// owned reports or serialized strings. It retains no references, performs no
// IO or global mutation, and callers should treat qualityIssues and
// needsHumanReview as informational output rather than an implicit publish action.

struct VoiceAssetQualityIssue {
    std::string code;
    std::string message;
    int severity = 0; // 0=info, 1=warning, 2=review before publish
};

struct VoiceAssetOrganization {
    std::string searchableDescription;
    std::string keywordCsv;
    std::vector<std::string> keywords;
    std::vector<VoiceAssetQualityIssue> qualityIssues;
    bool needsHumanReview = false;
};

class VoiceAssetOrganizer {
public:
    static VoiceAssetOrganization Analyze(const std::string& fileName,
                                          const TranscriptionResult& transcript,
                                          float durationSeconds);

    static std::string BuildSrt(const TranscriptionResult& transcript);
    static std::string BuildCsv(const TranscriptionResult& transcript);
    static std::string BuildJson(const TranscriptionResult& transcript);
};
