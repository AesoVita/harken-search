#include "AITranscriptionResultStateAdapter.h"

#include "AppState.h"
#include "AIEngine.h"
#include "TagSystem.h"
#include "VoiceAssetOrganizer.h"

#include <vector>

namespace aura::ai {

AITranscriptionResultStateAdapter::AITranscriptionResultStateAdapter(
    AppState& state,
    AIEngine& engine)
    : state_(state), engine_(engine) {}

void AITranscriptionResultStateAdapter::pump() {
    if (state_.aiTranscriptionTaskFileIds.empty()) {
        return;
    }

    std::vector<int> finishedTasks;
    finishedTasks.reserve(state_.aiTranscriptionTaskFileIds.size());

    for (const auto& [taskId, fileId] :
         state_.aiTranscriptionTaskFileIds) {
        AITask task = engine_.GetTask(taskId);
        if (task.status != AITaskStatus::Completed &&
            task.status != AITaskStatus::Failed &&
            task.status != AITaskStatus::Cancelled) {
            continue;
        }

        uint64_t taskGeneration = 0;
        auto genIt = state_.aiTranscriptionTaskGenerations.find(taskId);
        if (genIt != state_.aiTranscriptionTaskGenerations.end()) {
            taskGeneration = genIt->second;
        }
        const bool isLatestForFile =
            fileId <= 0 ||
            state_.aiTranscriptionFileGenerations[fileId] == taskGeneration;

        if (isLatestForFile &&
            task.status == AITaskStatus::Completed &&
            task.transcription.success &&
            fileId > 0) {
            state_.aiTranscriptCache[fileId] =
                task.transcription.fullText;
            state_.aiTranscriptResultCache[fileId] = task.transcription;
            state_.aiSelectedTranscriptSegment = -1;

            std::string fileName;
            auto nameIt =
                state_.aiTranscriptionTaskFileNames.find(taskId);
            if (nameIt != state_.aiTranscriptionTaskFileNames.end()) {
                fileName = nameIt->second;
            }

            std::string keywords;
            auto kwIt =
                state_.aiTranscriptionTaskKeywords.find(taskId);
            if (kwIt != state_.aiTranscriptionTaskKeywords.end()) {
                keywords = kwIt->second;
            }

            float durationSeconds = 0.0f;
            AudioRecord sourceRecord =
                state_.database.getFileById(fileId);
            if (sourceRecord.id > 0) {
                durationSeconds = sourceRecord.durationSeconds;
            }
            state_.aiVoiceAssetOrganizationCache[fileId] =
                VoiceAssetOrganizer::Analyze(
                    fileName,
                    task.transcription,
                    durationSeconds);

            auto suggestions =
                TagSystemManager::suggestTagsFromText(
                    fileName,
                    task.transcription.fullText,
                    keywords,
                    10);
            auto& cache = state_.aiTagSuggestionCache[fileId];
            cache.clear();
            for (const auto& suggestion : suggestions) {
                cache.push_back(
                    {
                        suggestion.value,
                        suggestion.value_zh,
                        static_cast<int>(suggestion.dimension),
                        suggestion.confidence,
                    });
            }
        }

        finishedTasks.push_back(taskId);
    }

    for (int taskId : finishedTasks) {
        state_.aiTranscriptionTaskFileIds.erase(taskId);
        state_.aiTranscriptionTaskGenerations.erase(taskId);
        state_.aiTranscriptionTaskFileNames.erase(taskId);
        state_.aiTranscriptionTaskKeywords.erase(taskId);
        if (state_.aiCurrentTranscriptionTaskId == taskId) {
            state_.aiCurrentTranscriptionTaskId = -1;
            state_.aiCurrentTranscriptionFileId = 0;
        }
    }
}

}  // namespace aura::ai
