#pragma once

// @stage7-contract
// API contract: cover replacement foundation types carry value/state snapshots
// and workflow decisions; they do not own UI textures or database connections.
// Callers preserve the existing UI-thread and persistence boundaries.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct SearchFilter;

namespace aura::cover_replacement {

enum class CoverStorageCompatibilityMode {
    DbFirstDualStorage,
};

inline constexpr CoverStorageCompatibilityMode kSelectedCoverStorageCompatibilityMode =
    CoverStorageCompatibilityMode::DbFirstDualStorage;

struct CoverStorageBridgeContract {
    CoverStorageCompatibilityMode mode = CoverStorageCompatibilityMode::DbFirstDualStorage;
    bool legacyDbKeyRequired = true;
    bool replacementCanonicalIdAllowed = true;
    bool dualReadRequired = true;
    bool dualWriteRequired = true;
    bool rollbackUsesDbTruth = true;
};

inline constexpr CoverStorageBridgeContract kSelectedCoverStorageBridgeContract{};

enum class CoverIdentityState {
    Preliminary,
    LegacyKey,
    Canonical,
    Unknown,
};

enum class CoverMigrationBridgeState {
    RollbackReadyDualStorage,
};

struct CoverIdentityRecord {
    std::string identityKey;
    std::string sourcePath;
    std::optional<std::string> canonicalId;
    CoverMigrationBridgeState migrationState =
        CoverMigrationBridgeState::RollbackReadyDualStorage;
};

bool IsPreliminaryCoverIdentity(std::string_view identityKey);
bool IsLegacyCoverIdentityKey(std::string_view identityKey);
CoverIdentityState ClassifyCoverIdentityState(std::string_view identityKey);
std::string NormalizeIdentityKeyForComparison(std::string_view identityKey);
bool HasPersistentFacetFilterSemantics(const SearchFilter& filter);
bool ShouldPersistFacetEntriesForFilter(const SearchFilter& filter, int entryCount);
bool IsFilteredRefreshCountStateImpossible(int loadedCount,
                                           int64_t totalCount,
                                           bool hasActiveDbFilter);
bool IsFilteredFacetWarmStartCountStateImpossible(int64_t totalCount,
                                                  bool hasActiveDbFilter,
                                                  bool facetsComputed,
                                                  bool hasAnyFacetPayload);

}
