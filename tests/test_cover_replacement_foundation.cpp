#include "src/CoverReplacementFoundation.h"
#include "src/Database.h"
#include "src/SearchQuery.h"

#include <cstdio>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    using namespace aura::cover_replacement;

    if (!Check(kSelectedCoverStorageCompatibilityMode ==
                   CoverStorageCompatibilityMode::DbFirstDualStorage,
               "selected compatibility mode should stay DbFirstDualStorage")) {
        return 1;
    }
    if (!Check(kSelectedCoverStorageBridgeContract.mode ==
                   CoverStorageCompatibilityMode::DbFirstDualStorage &&
                   kSelectedCoverStorageBridgeContract.legacyDbKeyRequired &&
                   kSelectedCoverStorageBridgeContract.replacementCanonicalIdAllowed &&
                   kSelectedCoverStorageBridgeContract.dualReadRequired &&
                   kSelectedCoverStorageBridgeContract.dualWriteRequired &&
                   kSelectedCoverStorageBridgeContract.rollbackUsesDbTruth,
               "selected bridge contract should lock dual-read dual-write rollback-safe DB truth")) {
        return 1;
    }

    CoverIdentityRecord preliminaryRecord{
        "?|folder",
        "C:/Lib/Folder/cover.jpg",
        std::nullopt,
        CoverMigrationBridgeState::RollbackReadyDualStorage,
    };
    if (!Check(preliminaryRecord.identityKey == "?|folder" &&
                   preliminaryRecord.sourcePath == "C:/Lib/Folder/cover.jpg" &&
                   !preliminaryRecord.canonicalId.has_value() &&
                   preliminaryRecord.migrationState ==
                       CoverMigrationBridgeState::RollbackReadyDualStorage,
               "identity record should retain bridge-ready replacement fields")) {
        return 1;
    }

    if (!Check(IsPreliminaryCoverIdentity("?|folder"),
               "preliminary ?|folder identity should be recognized")) {
        return 1;
    }
    if (!Check(ClassifyCoverIdentityState("?|folder") ==
                   CoverIdentityState::Preliminary,
               "preliminary identity should classify as Preliminary")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("?|folder") == "?|folder",
               "preliminary comparison key should stay stable")) {
        return 1;
    }

    if (!Check(IsLegacyCoverIdentityKey("F|C:/Lib/Folder/cover.jpg"),
               "legacy F|path key should be recognized")) {
        return 1;
    }
    if (!Check(!IsPreliminaryCoverIdentity("F|C:/Lib/Folder/cover.jpg") &&
                   ClassifyCoverIdentityState("F|C:/Lib/Folder/cover.jpg") ==
                       CoverIdentityState::LegacyKey,
               "legacy F|path should not be treated as preliminary")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|C:/Lib/Folder/cover.jpg") ==
                   "F|PATH|c:\\lib\\folder\\cover.jpg",
               "legacy comparison key should normalize to F|PATH| form")) {
        return 1;
    }

    if (!Check(!IsPreliminaryCoverIdentity("E|abc123") &&
                   !IsLegacyCoverIdentityKey("E|abc123") &&
                   ClassifyCoverIdentityState("E|abc123") ==
                       CoverIdentityState::Canonical,
               "embedded E| key should not be misclassified")) {
        return 1;
    }
    if (!Check(!IsPreliminaryCoverIdentity("F|H|abcd|42") &&
                   !IsLegacyCoverIdentityKey("F|H|abcd|42") &&
                   ClassifyCoverIdentityState("F|H|abcd|42") ==
                       CoverIdentityState::Canonical,
               "canonical F|H| key should not be misclassified")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|PATH|C:/Lib/Folder/cover.jpg") ==
                   "F|PATH|c:\\lib\\folder\\cover.jpg",
               "canonical path keys should normalize for comparison")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|PATH|C:/Lib/Folder/cover.jpg|1700") ==
                   "F|PATH|c:\\lib\\folder\\cover.jpg|1700",
               "canonical path keys should preserve mtime suffix")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|C:/Lib/Folder/cover.jpg") ==
                   NormalizeIdentityKeyForComparison("F|PATH|c:\\lib\\folder\\cover.jpg"),
               "legacy and canonical folder-path keys should compare equal")) {
        return 1;
    }
    if (!Check(ClassifyCoverIdentityState("F|PATH|C:/Lib/Folder/cover.jpg|1700") ==
                   CoverIdentityState::Canonical,
               "canonical path keys with mtime should stay canonical")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|H|abcd|42") == "F|H|abcd|42",
               "canonical hash keys should stay stable")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|H|") == "" &&
                   NormalizeIdentityKeyForComparison("F|H||42") == "" &&
                   NormalizeIdentityKeyForComparison("F|H|abcd") == "",
               "malformed canonical hash keys should be rejected")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("F|PATH|") == "" &&
                   NormalizeIdentityKeyForComparison("F|PATH||42") == "" &&
                   NormalizeIdentityKeyForComparison("F|") == "",
               "empty canonical and legacy path keys should be rejected")) {
        return 1;
    }
    if (!Check(ClassifyCoverIdentityState("X|mystery") == CoverIdentityState::Unknown &&
                   NormalizeIdentityKeyForComparison("X|mystery") == "",
               "unknown identity prefixes should not be upgraded into replacement truth")) {
        return 1;
    }
    if (!Check(NormalizeIdentityKeyForComparison("?|Folder/Sub") == "?|Folder/Sub",
               "preliminary identity should remain byte-stable during comparison")) {
        return 1;
    }

    SearchFilter emptyFilter;
    if (!Check(!HasPersistentFacetFilterSemantics(emptyFilter),
               "empty filter should not claim filtered facet-cache persistence semantics")) {
        return 1;
    }
    if (!Check(!ShouldPersistFacetEntriesForFilter(emptyFilter, 499) &&
                   ShouldPersistFacetEntriesForFilter(emptyFilter, 500),
               "unfiltered facet persistence should stay gated by the large-entry threshold")) {
        return 1;
    }

    SearchFilter keywordFilter;
    keywordFilter.keyword = "door";
    if (!Check(HasPersistentFacetFilterSemantics(keywordFilter) &&
                   ShouldPersistFacetEntriesForFilter(keywordFilter, 1),
               "keyword filter should always opt into persistent facet-cache semantics")) {
        return 1;
    }

    std::vector<SynonymGroup> synonymGroups = {
        SynonymGroup{{"hit"}, {"impact"}}
    };
    SearchFilter synonymFilter;
    synonymFilter.synonyms = &synonymGroups;
    if (!Check(HasPersistentFacetFilterSemantics(synonymFilter),
               "synonym-expanded queries should share the same persistent facet-cache policy")) {
        return 1;
    }

    if (!Check(IsFilteredRefreshCountStateImpossible(3, 0, true) &&
                   !IsFilteredRefreshCountStateImpossible(0, 0, true) &&
                   !IsFilteredRefreshCountStateImpossible(3, 7, true) &&
                   !IsFilteredRefreshCountStateImpossible(3, 0, false),
               "refresh impossible-state helper should only flag positive rows with zero total inside filtered scopes")) {
        return 1;
    }

    if (!Check(IsFilteredFacetWarmStartCountStateImpossible(0, true, true, true) &&
                   !IsFilteredFacetWarmStartCountStateImpossible(1, true, true, true) &&
                   !IsFilteredFacetWarmStartCountStateImpossible(0, false, true, true) &&
                   !IsFilteredFacetWarmStartCountStateImpossible(0, true, false, true) &&
                   !IsFilteredFacetWarmStartCountStateImpossible(0, true, true, false),
               "facet warm-start impossible-state helper should only flag zero-total facet payloads inside filtered scopes")) {
        return 1;
    }

    std::printf("PASS: cover replacement foundation contract holds\n");
    return 0;
}
