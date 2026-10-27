#pragma once
// SimilaritySearch.h — Find similar audio files by feature distance (F-028)

#include "FeatureExtractor.h"
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <shared_mutex>

// @stage7-contract
// API contract: SimilaritySearch owns its feature cache and synchronously
// mutates it through load/update/remove operations. Database and feature
// inputs are borrowed per call; findSimilar is safe for concurrent readers
// after cache publication and returns owned results. Callers must coordinate
// lifecycle with the owning worker and treat cache readiness as explicit.

// Forward declaration
class Database;

// Result

struct SimilarityResult {
    int64_t  fileId     = 0;
    float    distance   = 0.0f;     // Lower = more similar
    float    similarity = 0.0f;     // 0..1, higher = more similar
    std::string filePath;
};

// Weight Presets

enum class SimilarityPreset : int {
    Balanced = 0,      // Equal weight across all dimensions
    TimbreFocused,     // MFCC features weighted 2x
    RhythmFocused,     // Energy/ADSR features weighted 2.5x
    PitchFocused,      // Pitch features weighted 3x
    SpectralFocused    // Spectral shape features weighted 2.5x
};

// Similarity Search Engine

class SimilaritySearch {
public:
    struct SimilarityFeatureRecord {
        int64_t fileId = 0;
        std::string filePath;
        AudioFeatureVector features;
    };

    SimilaritySearch() = default;

    // Load all feature vectors from DB into memory cache.
    // Call on background thread (may take 1-2s for 1M files).
    void loadFeatureCache(Database& db);

    // Replace the complete feature cache, dropping non-finite vectors.
    void replaceFeatureCache(std::vector<SimilarityFeatureRecord> entries);

    // Check if cache is ready
    bool isCacheLoaded() const;
    size_t cacheSize() const;

    // Add/update a single entry (for incremental updates during scan)
    void updateCacheEntry(int64_t fileId, const std::string& filePath,
                          const AudioFeatureVector& features);

    // Remove an entry (file deleted)
    void removeCacheEntry(int64_t fileId);

    // Find top-K similar files to the given query vector.
    // Thread-safe (uses shared_mutex for read access).
    std::vector<SimilarityResult> findSimilar(
        const AudioFeatureVector& query,
        int64_t excludeFileId = -1,  // Exclude the query file itself
        int topK = 50,
        SimilarityPreset preset = SimilarityPreset::Balanced,
        std::string_view excludeFilePath = {}  // Also exclude path aliases
    ) const;

    // Recompute normalization stats from current cache
    void recomputeStats();

private:
    struct CacheEntry {
        int64_t fileId = 0;
        std::string filePath;
        uint64_t pathIdentityHash = 0;
        float features[AudioFeatureVector::DIMS] = {};
    };

    // Feature cache (loaded from DB)
    std::vector<CacheEntry> m_cache;
    mutable std::shared_mutex m_cacheMutex;

    // Per-dimension normalization stats
    float m_mean[AudioFeatureVector::DIMS] = {};
    float m_invStd[AudioFeatureVector::DIMS] = {};  // 1/σ (precomputed)
    bool m_statsValid = false;
    bool m_cacheLoaded = false;

    // Caller must hold m_cacheMutex exclusively.
    void recomputeStatsLocked();

    // Compute weighted distance between two feature vectors
    float computeDistance(const float* a, const float* b,
                          const float* weights) const;

    // Get weight array for a given preset
    static void getWeights(SimilarityPreset preset, float* weights,
                           float voicedRatio);
};
