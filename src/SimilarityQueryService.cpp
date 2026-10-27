#include "SimilarityQueryService.h"

#include "Database.h"
#include "DiagnosticsLogger.h"
#include "FeatureExtractor.h"
#include "platform/filesystem.h"

#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace HarkenSearch {
namespace {

bool IsCancelled(const SimilarityWorkflow::CancelCheck& cancelled) {
    return cancelled && cancelled();
}

} // namespace

SimilarityQueryService::SimilarityQueryService(SimilaritySearch& cache)
    : cache_(cache) {}

SimilarityQueryService::~SimilarityQueryService() = default;

bool SimilarityQueryService::EnsureDatabase(const std::string& path) {
    if (path.empty() ||
        !Platform::FileSystem::ProbeRegularFile(path).isRegularFile) {
        return false;
    }

    if (database_ && databasePath_ == path && database_->isOpen()) return true;

    if (database_) database_->close();
    database_.reset();
    databasePath_.clear();
    cachedFeatureCount_ = -1;

    auto database = std::make_unique<Database>();
    if (!database->open(path, false)) return false;

    database_ = std::move(database);
    databasePath_ = path;
    return true;
}

SimilarityCompletion SimilarityQueryService::Execute(
    const SimilarityRequest& request,
    const SimilarityWorkflow::CancelCheck& cancelled) {
    SimilarityCompletion completion;
    completion.generation = request.generation;
    completion.reference = request.reference;

    DIAG_ACTION_FMT("Similarity", "request_start gen=%llu ref=%lld preset=%d",
                    static_cast<unsigned long long>(request.generation),
                    static_cast<long long>(request.reference.fileId),
                    static_cast<int>(request.preset));

    int indexedFeatureCount = 0;
    float cacheMilliseconds = 0.0f;
    bool cacheReloadActive = false;
    std::chrono::steady_clock::time_point cacheReloadStart;
    auto finishCacheReloadTimer = [&] {
        if (!cacheReloadActive) return;
        cacheMilliseconds = std::chrono::duration<float, std::milli>(
                                std::chrono::steady_clock::now() -
                                cacheReloadStart)
                                .count();
        cacheReloadActive = false;
    };
    auto fail = [&](SimilarityWorkflowError error) {
        finishCacheReloadTimer();
        completion.rows.clear();
        completion.error = error;
        DIAG_ACTION_FMT(
            "Similarity",
            "request_failed gen=%llu ref=%lld candidates=%d cache_ms=%.2f query_ms=%.2f rows=%d error=%d",
            static_cast<unsigned long long>(completion.generation),
            static_cast<long long>(completion.reference.fileId),
            indexedFeatureCount, cacheMilliseconds,
            completion.queryMilliseconds,
            static_cast<int>(completion.rows.size()),
            static_cast<int>(error));
        return completion;
    };

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    if (request.generation == 0) return fail(SimilarityWorkflowError::SearchFailed);
    if (request.databasePath.empty()) {
        return fail(SimilarityWorkflowError::DatabaseUnavailable);
    }
    if (request.reference.fileId <= 0 || request.reference.filePath.empty()) {
        return fail(SimilarityWorkflowError::ReferenceUnreadable);
    }
    if (request.topK <= 0) return fail(SimilarityWorkflowError::NoCandidates);

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    if (!EnsureDatabase(request.databasePath)) {
        return fail(SimilarityWorkflowError::DatabaseUnavailable);
    }

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    AudioFeatureVector referenceFeature;
    const bool hasValidReferenceFeature =
        database_->getAudioFeaturesById(request.reference.fileId,
                                        referenceFeature) &&
        referenceFeature.isValid();
    if (!hasValidReferenceFeature) {
        if (IsCancelled(cancelled)) {
            return fail(SimilarityWorkflowError::Cancelled);
        }

        const AudioRecord record =
            database_->getFileById(request.reference.fileId);
        if (IsCancelled(cancelled)) {
            return fail(SimilarityWorkflowError::Cancelled);
        }
        if (record.id <= 0 || record.filePath.empty()) {
            return fail(SimilarityWorkflowError::ReferenceUnreadable);
        }

        const FeatureExtractionResult extracted =
            FeatureExtractor::extractFromFileChecked(record.filePath, cancelled);
        if (IsCancelled(cancelled) ||
            extracted.error == FeatureExtractionError::Cancelled) {
            return fail(SimilarityWorkflowError::Cancelled);
        }
        if (!extracted.ok) {
            return fail(SimilarityWorkflowError::ReferenceUnreadable);
        }

        referenceFeature = extracted.features;
        float integratedLufs = -70.0f;
        float momentaryLufs = -70.0f;
        float shortTermLufs = -70.0f;
        float truePeakDbtp = -70.0f;
        database_->getLoudnessByPath(record.filePath, integratedLufs,
                                     momentaryLufs, shortTermLufs,
                                     truePeakDbtp);
        const float trueRmsDbfs =
            referenceFeature.features[AudioFeatureVector::ENERGY_OFFSET];
        FeatureExtractor::fillGlobalFeatures(
            referenceFeature, integratedLufs, trueRmsDbfs,
            record.durationSeconds, truePeakDbtp);
        if (!referenceFeature.isValid()) {
            return fail(SimilarityWorkflowError::ReferenceUnreadable);
        }

        if (IsCancelled(cancelled)) {
            return fail(SimilarityWorkflowError::Cancelled);
        }
        if (!database_->saveAudioFeaturesByPath(record.filePath,
                                                referenceFeature)) {
            return fail(SimilarityWorkflowError::SearchFailed);
        }
        cache_.updateCacheEntry(record.id, record.filePath, referenceFeature);
    }

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    int featureCount = 0;
    if (!database_->tryCountFilesWithFeatures(featureCount)) {
        return fail(SimilarityWorkflowError::SearchFailed);
    }
    indexedFeatureCount = featureCount;
    if (featureCount != cachedFeatureCount_) {
        if (IsCancelled(cancelled)) {
            return fail(SimilarityWorkflowError::Cancelled);
        }
        cacheReloadStart = std::chrono::steady_clock::now();
        cacheReloadActive = true;
        std::vector<Database::AudioFeatureEntry> databaseEntries;
        if (!database_->tryLoadAllAudioFeatures(databaseEntries)) {
            return fail(SimilarityWorkflowError::SearchFailed);
        }
        if (databaseEntries.size() != static_cast<size_t>(featureCount)) {
            return fail(SimilarityWorkflowError::SearchFailed);
        }

        std::vector<SimilaritySearch::SimilarityFeatureRecord> cacheEntries;
        cacheEntries.reserve(databaseEntries.size());
        for (Database::AudioFeatureEntry& databaseEntry : databaseEntries) {
            if (!databaseEntry.features.isValid()) {
                return fail(SimilarityWorkflowError::SearchFailed);
            }
            SimilaritySearch::SimilarityFeatureRecord cacheEntry;
            cacheEntry.fileId = databaseEntry.fileId;
            cacheEntry.filePath = std::move(databaseEntry.filePath);
            cacheEntry.features = databaseEntry.features;
            cacheEntries.push_back(std::move(cacheEntry));
        }

        if (IsCancelled(cancelled)) {
            return fail(SimilarityWorkflowError::Cancelled);
        }
        cache_.replaceFeatureCache(std::move(cacheEntries));
        cachedFeatureCount_ = featureCount;
        finishCacheReloadTimer();
    }

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    const auto queryStart = std::chrono::steady_clock::now();
    const std::vector<SimilarityResult> ranked = cache_.findSimilar(
        referenceFeature, request.reference.fileId, request.topK,
        request.preset, request.reference.filePath);
    completion.queryMilliseconds = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - queryStart).count();
    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    if (ranked.empty()) return fail(SimilarityWorkflowError::NoCandidates);

    std::vector<int64_t> ids;
    ids.reserve(ranked.size());
    for (const SimilarityResult& result : ranked) ids.push_back(result.fileId);

    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);
    std::vector<AudioRecord> records = database_->getFilesByIds(ids);
    if (IsCancelled(cancelled)) return fail(SimilarityWorkflowError::Cancelled);

    std::unordered_map<int64_t, AudioRecord> recordsById;
    recordsById.reserve(records.size());
    for (AudioRecord& record : records) {
        recordsById.emplace(record.id, std::move(record));
    }

    completion.rows.reserve(ranked.size());
    for (const SimilarityResult& result : ranked) {
        const auto it = recordsById.find(result.fileId);
        if (it == recordsById.end()) continue;
        const AudioRecord& record = it->second;
        SimilarityDisplayRow row;
        row.fileId = record.id;
        row.filePath = record.filePath;
        row.fileName = record.fileName;
        row.durationSeconds = record.durationSeconds;
        row.format = record.format;
        row.similarity = result.similarity;
        completion.rows.push_back(std::move(row));
    }

    if (completion.rows.empty()) {
        return fail(SimilarityWorkflowError::NoCandidates);
    }
    completion.error = SimilarityWorkflowError::None;
    finishCacheReloadTimer();
    DIAG_ACTION_FMT(
        "Similarity",
        "request_done gen=%llu candidates=%d rows=%d cache_ms=%.2f query_ms=%.2f",
        static_cast<unsigned long long>(completion.generation),
        indexedFeatureCount, static_cast<int>(completion.rows.size()),
        cacheMilliseconds, completion.queryMilliseconds);
    return completion;
}

SimilarityWorkflow::Handler MakeSimilarityQueryHandler(SimilaritySearch& cache) {
    auto service = std::make_shared<SimilarityQueryService>(cache);
    return [service](const SimilarityRequest& request,
                     const SimilarityWorkflow::CancelCheck& cancelled) {
        return service->Execute(request, cancelled);
    };
}

} // namespace HarkenSearch
