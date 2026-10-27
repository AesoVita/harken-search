#pragma once

#include "SearchQuery.h"

#include <cstdint>
#include <string>
#include <vector>

// FileTableV6 physical indices used by search-hit column reveal.
// Keep these aligned with TableSetupColumn order in src/main.cpp.
enum class SearchHitTableColumn : int {
    FileName = 0,
    Description = 6,
    Keywords = 7,
    Category = 9,
    Artist = 10,
    Album = 11,
    FilePath = 16,
};

// Mirrors SearchMatchSource without pulling Database.h into this unit.
enum class SearchHitMatchSource : uint8_t {
    None = 0,
    Name,
    Path,
    Description,
    Keywords,
    Metadata,
    FuzzyName
};

struct SearchHitColumnRecord {
    SearchHitMatchSource matchSource = SearchHitMatchSource::None;
    std::string title;
    std::string artist;
    std::string album;
    std::string category;
};

inline int ColumnIndexForSearchField(const std::string& field) {
    if (field == "artist") return static_cast<int>(SearchHitTableColumn::Artist);
    if (field == "album") return static_cast<int>(SearchHitTableColumn::Album);
    if (field == "title" || field == "name") return static_cast<int>(SearchHitTableColumn::FileName);
    if (field == "category") return static_cast<int>(SearchHitTableColumn::Category);
    if (field == "keywords") return static_cast<int>(SearchHitTableColumn::Keywords);
    if (field == "desc" || field == "description") return static_cast<int>(SearchHitTableColumn::Description);
    if (field == "path") return static_cast<int>(SearchHitTableColumn::FilePath);
    return -1;
}

inline bool ContainsAsciiInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) return false;
    auto fold = [](unsigned char c) -> char {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    };
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool matched = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (fold(static_cast<unsigned char>(haystack[i + j])) !=
                fold(static_cast<unsigned char>(needle[j]))) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

inline int ResolveUnprefixedMetadataColumn(const SearchHitColumnRecord& record, const std::string& query) {
    // Title has no dedicated results column. Never treat a title/snippet hit as
    // filename column 0; prefer a real metadata column when the term is there.
    const std::vector<SearchQueryChip> chips = BuildSearchQueryChips(query);
    auto fieldHasTerm = [&](const std::string& field) {
        if (field.empty()) return false;
        if (ContainsAsciiInsensitive(field, query)) return true;
        for (const SearchQueryChip& chip : chips) {
            if (chip.kind != SearchQueryChipKind::Keyword) continue;
            if (!chip.label.empty() && ContainsAsciiInsensitive(field, chip.label)) return true;
        }
        return false;
    };
    if (fieldHasTerm(record.artist)) return static_cast<int>(SearchHitTableColumn::Artist);
    if (fieldHasTerm(record.album)) return static_cast<int>(SearchHitTableColumn::Album);
    if (fieldHasTerm(record.category)) return static_cast<int>(SearchHitTableColumn::Category);
    return -1;
}

inline int ResolveSearchHitColumnIndex(const SearchHitColumnRecord& record, const std::string& query) {
    for (const SearchQueryChip& chip : BuildSearchQueryChips(query)) {
        if (chip.kind != SearchQueryChipKind::Field) continue;
        const int column = ColumnIndexForSearchField(chip.field);
        if (column >= 0) return column;
    }

    switch (record.matchSource) {
    case SearchHitMatchSource::None:
        return -1;
    case SearchHitMatchSource::Name:
    case SearchHitMatchSource::FuzzyName:
        return static_cast<int>(SearchHitTableColumn::FileName);
    case SearchHitMatchSource::Description:
        return static_cast<int>(SearchHitTableColumn::Description);
    case SearchHitMatchSource::Keywords:
        return static_cast<int>(SearchHitTableColumn::Keywords);
    case SearchHitMatchSource::Path:
        return static_cast<int>(SearchHitTableColumn::FilePath);
    case SearchHitMatchSource::Metadata:
        return ResolveUnprefixedMetadataColumn(record, query);
    default:
        return -1;
    }
}

inline bool ShouldRevealSearchHitColumn(const std::string& query,
                                        uint64_t requestId,
                                        const std::string& lastQuery,
                                        uint64_t lastRequestId) {
    (void)requestId;
    (void)lastRequestId;
    if (query.empty()) return false;
    // Load-more and filter refreshes keep the same query while minting a new
    // request id. Reveal only on a new query, never on generation bumps.
    return query != lastQuery;
}
