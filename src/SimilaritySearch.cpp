// SimilaritySearch.cpp — Find similar audio files by feature distance (F-028)

#include "SimilaritySearch.h"
#include "Database.h"
#include "platform/text_encoding.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <numeric>
#include <string_view>

namespace {

struct PathIdentityScratch {
    std::string folded;
    std::string normalized;
    std::vector<std::pair<size_t, size_t>> segments;
#ifdef _WIN32
    std::string uppercaseUtf8;
    std::wstring wide;
    std::wstring wideUpper;
#endif
};

bool StartsWithAsciiCaseInsensitive(std::string_view text,
                                    std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    for (size_t index = 0; index < prefix.size(); ++index) {
        unsigned char lhs = static_cast<unsigned char>(text[index]);
        unsigned char rhs = static_cast<unsigned char>(prefix[index]);
        if (lhs >= 'A' && lhs <= 'Z') lhs = lhs - 'A' + 'a';
        if (rhs >= 'A' && rhs <= 'Z') rhs = rhs - 'A' + 'a';
        if (lhs != rhs) return false;
    }
    return true;
}

std::string_view NormalizePathIdentity(std::string_view rawPath,
                                       PathIdentityScratch& scratch) {
    scratch.folded.clear();
    scratch.normalized.clear();
    scratch.segments.clear();
    if (rawPath.empty()) return {};

    size_t sourceStart = 0;
#ifdef _WIN32
    if (StartsWithAsciiCaseInsensitive(rawPath, "\\\\?\\UNC\\")) {
        scratch.folded = "\\\\";
        sourceStart = 8;
    } else if (StartsWithAsciiCaseInsensitive(rawPath, "\\\\?\\")) {
        sourceStart = 4;
    }
#endif
    scratch.folded.reserve(rawPath.size());
    bool hasNonAscii = false;
    for (size_t index = sourceStart; index < rawPath.size(); ++index) {
        unsigned char byte = static_cast<unsigned char>(rawPath[index]);
#ifdef _WIN32
        if (byte == '/') byte = '\\';
        if (byte >= 'a' && byte <= 'z') byte = byte - 'a' + 'A';
        hasNonAscii = hasNonAscii || byte >= 0x80;
#else
        // POSIX keeps backslashes as ordinary filename bytes; only '/' is a
        // path separator on that platform.
#endif
        scratch.folded.push_back(static_cast<char>(byte));
    }
#ifdef _WIN32
    if (hasNonAscii &&
        Platform::TextEncoding::UppercaseWindowsInvariantUtf8(
            scratch.folded, scratch.uppercaseUtf8,
            scratch.wide, scratch.wideUpper)) {
        scratch.folded.swap(scratch.uppercaseUtf8);
    }
#endif

#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif
    const std::string_view folded = scratch.folded;
    size_t cursor = 0;
    bool absolute = false;
    bool driveRelative = false;
    size_t protectedSegments = 0;

#ifdef _WIN32
    if (folded.size() >= 2 && folded[0] == separator &&
        folded[1] == separator) {
        scratch.normalized = "\\\\";
        cursor = 2;
        absolute = true;
        protectedSegments = 2;  // UNC server/share cannot be popped by ..
    } else if (folded.size() >= 2 && folded[1] == ':') {
        scratch.normalized.assign(folded.data(), 2);
        cursor = 2;
        if (cursor < folded.size() && folded[cursor] == separator) {
            scratch.normalized.push_back(separator);
            ++cursor;
            absolute = true;
        } else {
            driveRelative = true;
        }
    } else if (!folded.empty() && folded[0] == separator) {
        scratch.normalized.push_back(separator);
        cursor = 1;
        absolute = true;
    }
#else
    if (!folded.empty() && folded[0] == separator) {
        scratch.normalized.push_back(separator);
        cursor = 1;
        absolute = true;
    }
#endif

    while (cursor < folded.size()) {
        while (cursor < folded.size() && folded[cursor] == separator) ++cursor;
        const size_t begin = cursor;
        while (cursor < folded.size() && folded[cursor] != separator) ++cursor;
        const size_t length = cursor - begin;
        if (length == 0 || (length == 1 && folded[begin] == '.')) continue;

        const bool parent = length == 2 && folded[begin] == '.' &&
                            folded[begin + 1] == '.';
        if (parent) {
            if (scratch.segments.size() > protectedSegments) {
                const auto [lastBegin, lastLength] = scratch.segments.back();
                const bool lastIsParent = lastLength == 2 &&
                    folded[lastBegin] == '.' && folded[lastBegin + 1] == '.';
                if (!lastIsParent) {
                    scratch.segments.pop_back();
                    continue;
                }
            }
            if (absolute) continue;
        }
        scratch.segments.emplace_back(begin, length);
    }

    bool firstSegment = true;
    for (const auto [begin, length] : scratch.segments) {
        const bool omitDriveRelativeSeparator =
            driveRelative && firstSegment && scratch.normalized.size() == 2;
        if (!scratch.normalized.empty() &&
            scratch.normalized.back() != separator &&
            !omitDriveRelativeSeparator) {
            scratch.normalized.push_back(separator);
        }
        scratch.normalized.append(folded.data() + begin, length);
        firstSegment = false;
    }
    return scratch.normalized;
}

uint64_t HashPathIdentity(std::string_view identity) {
    if (identity.empty()) return 0;
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char byte : identity) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

// Weight Presets

void SimilaritySearch::getWeights(SimilarityPreset preset, float* weights,
                                   float voicedRatio) {
    // Default: all 1.0
    for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
        weights[i] = 1.0f;

    // Global features slightly lower weight by default
    for (int i = AudioFeatureVector::GLOBAL_OFFSET; i < AudioFeatureVector::DIMS; ++i)
        weights[i] = 0.5f;

    switch (preset) {
    case SimilarityPreset::Balanced:
        break;

    case SimilarityPreset::TimbreFocused:
        for (int i = AudioFeatureVector::MFCC_OFFSET; i < AudioFeatureVector::MFCC_D_OFFSET + 13; ++i)
            weights[i] = 2.0f;
        for (int i = AudioFeatureVector::SPECTRAL_OFFSET; i < AudioFeatureVector::SPECTRAL_OFFSET + 5; ++i)
            weights[i] = 1.5f;
        for (int i = AudioFeatureVector::PITCH_OFFSET; i < AudioFeatureVector::PITCH_OFFSET + 3; ++i)
            weights[i] = 0.5f;
        for (int i = AudioFeatureVector::ENERGY_OFFSET; i < AudioFeatureVector::ENERGY_OFFSET + 10; ++i)
            weights[i] = 0.5f;
        break;

    case SimilarityPreset::RhythmFocused:
        for (int i = AudioFeatureVector::ENERGY_OFFSET; i < AudioFeatureVector::ENERGY_OFFSET + 10; ++i)
            weights[i] = 2.5f;
        for (int i = AudioFeatureVector::MFCC_OFFSET; i < AudioFeatureVector::MFCC_D_OFFSET + 13; ++i)
            weights[i] = 0.5f;
        for (int i = AudioFeatureVector::PITCH_OFFSET; i < AudioFeatureVector::PITCH_OFFSET + 3; ++i)
            weights[i] = 0.3f;
        break;

    case SimilarityPreset::PitchFocused:
        for (int i = AudioFeatureVector::PITCH_OFFSET; i < AudioFeatureVector::PITCH_OFFSET + 3; ++i)
            weights[i] = 3.0f;
        for (int i = AudioFeatureVector::MFCC_OFFSET; i < AudioFeatureVector::MFCC_D_OFFSET + 13; ++i)
            weights[i] = 0.5f;
        for (int i = AudioFeatureVector::ENERGY_OFFSET; i < AudioFeatureVector::ENERGY_OFFSET + 10; ++i)
            weights[i] = 0.3f;
        break;

    case SimilarityPreset::SpectralFocused:
        for (int i = AudioFeatureVector::SPECTRAL_OFFSET; i < AudioFeatureVector::SPECTRAL_OFFSET + 5; ++i)
            weights[i] = 2.5f;
        for (int i = AudioFeatureVector::MFCC_OFFSET; i < AudioFeatureVector::MFCC_D_OFFSET + 13; ++i)
            weights[i] = 1.0f;
        for (int i = AudioFeatureVector::PITCH_OFFSET; i < AudioFeatureVector::PITCH_OFFSET + 3; ++i)
            weights[i] = 1.0f;
        for (int i = AudioFeatureVector::ENERGY_OFFSET; i < AudioFeatureVector::ENERGY_OFFSET + 10; ++i)
            weights[i] = 0.5f;
        break;
    }

    // Voiced ratio adaptation: suppress pitch weights for noise/percussive sounds
    if (voicedRatio < 0.1f) {
        for (int i = AudioFeatureVector::PITCH_OFFSET; i < AudioFeatureVector::PITCH_OFFSET + 3; ++i)
            weights[i] = 0.0f;
        // Boost ADSR for percussive sounds
        for (int i = AudioFeatureVector::ENERGY_OFFSET; i < AudioFeatureVector::ENERGY_OFFSET + 10; ++i)
            weights[i] *= 1.5f;
    }
}

// Distance Computation

float SimilaritySearch::computeDistance(const float* a, const float* b,
                                         const float* weights) const {
    float sum = 0.0f;
    for (int i = 0; i < AudioFeatureVector::DIMS; ++i) {
        float diff = (a[i] - b[i]) * m_invStd[i];
        sum += weights[i] * diff * diff;
    }
    return sum;  // Return squared distance (sqrt not needed for ranking)
}

// Load Feature Cache from DB

void SimilaritySearch::loadFeatureCache(Database& db) {
    auto databaseEntries = db.loadAllAudioFeatures();
    std::vector<SimilarityFeatureRecord> entries;
    entries.reserve(databaseEntries.size());

    for (auto& databaseEntry : databaseEntries) {
        SimilarityFeatureRecord entry;
        entry.fileId = databaseEntry.fileId;
        entry.filePath = std::move(databaseEntry.filePath);
        entry.features = databaseEntry.features;
        entries.push_back(std::move(entry));
    }

    replaceFeatureCache(std::move(entries));
}

void SimilaritySearch::replaceFeatureCache(
    std::vector<SimilarityFeatureRecord> entries) {
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_cache.clear();
    m_cache.reserve(entries.size());

    PathIdentityScratch identityScratch;

    for (auto& entry : entries) {
        if (!entry.features.isValid()) continue;

        const std::string_view identity =
            NormalizePathIdentity(entry.filePath, identityScratch);
        const uint64_t identityHash = HashPathIdentity(identity);
        CacheEntry cacheEntry;
        cacheEntry.fileId = entry.fileId;
        cacheEntry.filePath = std::move(entry.filePath);
        cacheEntry.pathIdentityHash = identityHash;
        std::memcpy(cacheEntry.features, entry.features.features,
                    sizeof(cacheEntry.features));
        m_cache.push_back(std::move(cacheEntry));
    }

    m_cacheLoaded = true;
    recomputeStatsLocked();
}

bool SimilaritySearch::isCacheLoaded() const {
    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    return m_cacheLoaded;
}

size_t SimilaritySearch::cacheSize() const {
    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    return m_cache.size();
}

// Incremental Cache Updates

void SimilaritySearch::updateCacheEntry(int64_t fileId, const std::string& filePath,
                                         const AudioFeatureVector& features) {
    if (!features.isValid()) return;

    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);

    PathIdentityScratch identityScratch;
    const std::string_view identity =
        NormalizePathIdentity(filePath, identityScratch);
    const uint64_t identityHash = HashPathIdentity(identity);

    // Check if the file ID already exists. Path identity is cached for the
    // query hot path, but aliases remain in the feature cache so the legacy
    // ID-only exclusion contract can still resolve their own path.
    for (auto& entry : m_cache) {
        if (entry.fileId == fileId) {
            entry.filePath = filePath;
            entry.pathIdentityHash = identityHash;
            std::memcpy(entry.features, features.features, sizeof(entry.features));
            return;
        }
    }

    // New entry
    CacheEntry ce;
    ce.fileId = fileId;
    ce.filePath = filePath;
    ce.pathIdentityHash = identityHash;
    std::memcpy(ce.features, features.features, sizeof(ce.features));
    m_cache.push_back(std::move(ce));
}

void SimilaritySearch::removeCacheEntry(int64_t fileId) {
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_cache.erase(
        std::remove_if(m_cache.begin(), m_cache.end(),
                        [fileId](const CacheEntry& e) { return e.fileId == fileId; }),
        m_cache.end());
}

// Recompute Normalization Stats

void SimilaritySearch::recomputeStats() {
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    recomputeStatsLocked();
}

void SimilaritySearch::recomputeStatsLocked() {
    if (m_cache.empty()) {
        std::fill(m_mean, m_mean + AudioFeatureVector::DIMS, 0.0f);
        std::fill(m_invStd, m_invStd + AudioFeatureVector::DIMS, 1.0f);
        m_statsValid = false;
        return;
    }

    // Keep every database ID available for the public ID-only exclusion
    // contract, but let path aliases contribute to normalization once. Sort
    // only compact indexes (no million-node hash table); exact normalized
    // paths are compared inside equal-hash groups for collision safety.
    std::vector<size_t> order(m_cache.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](size_t lhs, size_t rhs) {
        return m_cache[lhs].pathIdentityHash < m_cache[rhs].pathIdentityHash;
    });
    std::vector<size_t> uniqueIndices;
    uniqueIndices.reserve(m_cache.size());
    std::vector<size_t> uniqueHashGroup;
    PathIdentityScratch currentScratch;
    PathIdentityScratch existingScratch;
    size_t groupStart = 0;
    while (groupStart < order.size()) {
        const uint64_t hash = m_cache[order[groupStart]].pathIdentityHash;
        size_t groupEnd = groupStart + 1;
        while (groupEnd < order.size() &&
               m_cache[order[groupEnd]].pathIdentityHash == hash) {
            ++groupEnd;
        }
        uniqueHashGroup.clear();
        for (size_t position = groupStart; position < groupEnd; ++position) {
            const size_t cacheIndex = order[position];
            if (hash == 0) {
                // Empty/invalid paths have no usable identity; retain them by
                // ID rather than accidentally collapsing unrelated records.
                uniqueIndices.push_back(cacheIndex);
                continue;
            }
            const std::string_view currentIdentity = NormalizePathIdentity(
                m_cache[cacheIndex].filePath, currentScratch);
            bool duplicate = false;
            for (const size_t existingIndex : uniqueHashGroup) {
                if (NormalizePathIdentity(m_cache[existingIndex].filePath,
                                          existingScratch) == currentIdentity) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                uniqueHashGroup.push_back(cacheIndex);
                uniqueIndices.push_back(cacheIndex);
            }
        }
        groupStart = groupEnd;
    }

    const size_t n = uniqueIndices.size();
    if (n < 2) {
        std::fill(m_mean, m_mean + AudioFeatureVector::DIMS, 0.0f);
        std::fill(m_invStd, m_invStd + AudioFeatureVector::DIMS, 1.0f);
        m_statsValid = false;
        return;
    }

    // Mean
    std::fill(m_mean, m_mean + AudioFeatureVector::DIMS, 0.0f);
    for (const size_t index : uniqueIndices)
        for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
            m_mean[i] += m_cache[index].features[i];
    for (int i = 0; i < AudioFeatureVector::DIMS; ++i)
        m_mean[i] /= (float)n;

    // Std dev
    float var[AudioFeatureVector::DIMS] = {};
    for (const size_t index : uniqueIndices)
        for (int i = 0; i < AudioFeatureVector::DIMS; ++i) {
            float d = m_cache[index].features[i] - m_mean[i];
            var[i] += d * d;
        }

    for (int i = 0; i < AudioFeatureVector::DIMS; ++i) {
        float stddev = std::sqrt(var[i] / (float)(n - 1));
        m_invStd[i] = (stddev > 1e-6f) ? (1.0f / stddev) : 1.0f;
    }

    m_statsValid = true;
}

// Find Similar

std::vector<SimilarityResult> SimilaritySearch::findSimilar(
    const AudioFeatureVector& query,
    int64_t excludeFileId,
    int topK,
    SimilarityPreset preset,
    std::string_view excludeFilePath
) const {
    std::vector<SimilarityResult> results;
    if (topK <= 0 || !query.isValid()) return results;

    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    if (m_cache.empty() || !m_statsValid) return results;

    if (excludeFilePath.empty() && excludeFileId > 0) {
        const auto it = std::find_if(
            m_cache.begin(), m_cache.end(),
            [excludeFileId](const CacheEntry& entry) {
                return entry.fileId == excludeFileId;
            });
        if (it != m_cache.end()) excludeFilePath = it->filePath;
    }
    PathIdentityScratch excludedScratch;
    const std::string_view excludedIdentity =
        NormalizePathIdentity(excludeFilePath, excludedScratch);
    const uint64_t excludedIdentityHash = HashPathIdentity(excludedIdentity);

    for (int i = 0; i < AudioFeatureVector::DIMS; ++i) {
        if (!std::isfinite(m_mean[i]) || !std::isfinite(m_invStd[i]))
            return results;
    }

    // Get weights for this preset
    float weights[AudioFeatureVector::DIMS];
    getWeights(preset, weights, query.voicedRatio());

    struct Candidate {
        float distance = 0.0f;
        size_t cacheIndex = 0;
        int64_t fileId = 0;
    };
    const size_t boundedTopK =
        std::min(static_cast<size_t>(topK), m_cache.size());
    std::vector<Candidate> best;
    best.reserve(boundedTopK);
    size_t worstIndex = 0;
    auto candidateIsBetter = [](const Candidate& lhs, const Candidate& rhs) {
        return lhs.distance < rhs.distance ||
               (lhs.distance == rhs.distance && lhs.fileId < rhs.fileId);
    };
    auto refreshWorst = [&] {
        if (best.empty()) {
            worstIndex = 0;
            return;
        }
        worstIndex = 0;
        for (size_t index = 1; index < best.size(); ++index) {
            if (candidateIsBetter(best[worstIndex], best[index])) {
                worstIndex = index;
            }
        }
    };
    PathIdentityScratch candidateScratch;
    PathIdentityScratch existingScratch;

    for (size_t idx = 0; idx < m_cache.size(); ++idx) {
        const auto& entry = m_cache[idx];
        if (entry.fileId == excludeFileId) continue;
        if (excludedIdentityHash != 0 &&
            entry.pathIdentityHash == excludedIdentityHash &&
            NormalizePathIdentity(entry.filePath, candidateScratch) ==
                excludedIdentity) {
            continue;
        }

        bool candidateIsFinite = true;
        for (int dim = 0; dim < AudioFeatureVector::DIMS; ++dim) {
            if (!std::isfinite(entry.features[dim])) {
                candidateIsFinite = false;
                break;
            }
        }
        if (!candidateIsFinite) continue;

        float dist = computeDistance(query.features, entry.features, weights);
        if (!std::isfinite(dist)) continue;

        Candidate candidate{dist, idx, entry.fileId};
        if (best.size() == boundedTopK &&
            !candidateIsBetter(candidate, best[worstIndex])) {
            continue;
        }

        bool replacedDuplicate = false;
        if (entry.pathIdentityHash != 0) {
            for (size_t index = 0; index < best.size(); ++index) {
                const auto& existing = m_cache[best[index].cacheIndex];
                if (existing.pathIdentityHash != entry.pathIdentityHash ||
                    NormalizePathIdentity(existing.filePath, existingScratch) !=
                        NormalizePathIdentity(entry.filePath, candidateScratch)) {
                    continue;
                }
                replacedDuplicate = true;
                if (candidateIsBetter(candidate, best[index])) {
                    best[index] = candidate;
                    refreshWorst();
                }
                break;
            }
        }
        if (replacedDuplicate) continue;

        if (best.size() < boundedTopK) {
            best.push_back(candidate);
        } else {
            best[worstIndex] = candidate;
        }
        refreshWorst();
    }

    results.reserve(best.size());
    for (const Candidate& candidate : best) {
        SimilarityResult result;
        result.fileId = m_cache[candidate.cacheIndex].fileId;
        result.filePath = m_cache[candidate.cacheIndex].filePath;
        result.distance = candidate.distance;
        results.push_back(std::move(result));
    }

    std::sort(results.begin(), results.end(),
              [](const SimilarityResult& lhs, const SimilarityResult& rhs) {
                  if (lhs.distance != rhs.distance)
                      return lhs.distance < rhs.distance;
                  return lhs.fileId < rhs.fileId;
              });

    for (auto& result : results) {
        const float distance = std::max(0.0f, result.distance);
        const float similarity = 1.0f / (1.0f + std::sqrt(distance));
        result.similarity = std::clamp(similarity, 0.0f, 1.0f);
    }

    return results;
}
