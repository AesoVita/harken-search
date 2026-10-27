#pragma once

#include <string>
#include <vector>

// Forward declarations
struct AppState;

// @stage7-contract
// API contract: query parsing returns owned FTS/filter strings safe for bound
// parameters. It performs no database mutation and reports malformed input via
// SearchQueryResult::ok/error while preserving caller input storage.

// FTS5 Query Parsing

struct SearchQueryResult {
    bool ok = true;
    std::string error;

    // FTS5 MATCH expression (safe to pass as a bound parameter)
    std::string ftsMatch;

    // Non-FTS filters extracted from the query text.
    // Currently used for path:... (applied as file_path LIKE %term%).
    std::vector<std::string> pathContains;

    // Name substring/suffix filter via trigram FTS index.
    // Bare words (>=3 Unicode characters, non-phrase, non-field-prefixed) route here for
    // fast token-internal substring match, e.g. "30017" -> "B30017_Gun.wav".
    // Applied as: id IN (SELECT rowid FROM audio_files_name_trigram WHERE audio_files_name_trigram MATCH ?)
    std::vector<std::string> nameContains;

    // Tag filter: tag:rock → find files that have this tag in file_tags table.
    // Applied as a JOIN/subquery, not FTS.
    std::vector<std::string> tagFilters;

    // Color filter: color:red → find files with track_colour = 'red'.
    std::vector<std::string> colorFilters;

    // Purely-negative query terms (e.g. "NOT hit NOT speech").
    // When set, ftsMatch is empty and these are applied as SQL NOT LIKE conditions.
    // FTS5 does not support standalone NOT without a preceding positive term.
    std::vector<std::string> excludeTerms;
};

enum class SearchQueryChipKind {
    Keyword,
    ExcludeKeyword,
    Field,
    ExcludeField,
    Operator
};

struct SearchQueryChip {
    SearchQueryChipKind kind = SearchQueryChipKind::Keyword;
    std::string field;
    std::string label;
    std::vector<std::string> sourceTokens;
};

// Synonym group for thesaurus-based query expansion.
// Lookup is BIDIRECTIONAL (case-insensitive ASCII fold):
//   - Hit on triggers -> expand query to OR with `related`.
//   - Hit on related  -> expand query to OR with `triggers` + the other `related`.
// Both lists are user-defined; the trigger/related labels remain only for
// editing/storage convenience. Expansion is capped at kMaxSynonymExpansion
// per token and stops at the first matching group to avoid explosion.
struct SynonymGroup {
    std::vector<std::string> triggers;
    std::vector<std::string> related;
};

// Parse a user query (power syntax) and generate a safe FTS5 MATCH expression.
//
// Supported:
// - default AND between terms/groups
// - OR keyword
// - parentheses: ( ... )
// - phrase: "..."
// - exclusion: -term / -"phrase" / -field:term
// - fields: artist:, album:, title:, category:, keywords:, name:, desc:, path:, tag:, color:
//
// Notes:
// - path: is NOT indexed in FTS; it is returned in pathContains and must be applied separately.
// - tag: filters by file_tags JOIN; returned in tagFilters.
// - color: (or colour:) filters by track_colour column; returned in colorFilters.
// exactMode: when true, tokens are NOT expanded with prefix '*' (precise search).
//            when false (default), all plain tokens get '*' for broader fuzzy recall.
// synonyms:  optional pointer to synonym groups for cross-language query expansion.
SearchQueryResult ParseSearchQueryToFts5(const std::string& input, bool exactMode = false,
                                         const std::vector<SynonymGroup>* synonyms = nullptr);

// Build visible, removable chips for typed search syntax. This is UI-only
// metadata; query execution still goes through ParseSearchQueryToFts5().
std::vector<SearchQueryChip> BuildSearchQueryChips(const std::string& input);
std::string RemoveSearchQueryChipFromText(const std::string& input, const SearchQueryChip& chip);

// Search Management (implemented in main.cpp)

// The following functions manage search state and are called from UI code.
// They are currently implemented in main.cpp and may be moved here later.

// Refresh search results based on current filters
// If resetPagination is true, clears existing results and starts from offset 0
void RefreshSearchResults(AppState& state, bool resetPagination = true);

// Apply all active filters and refresh results
void ApplyFiltersAndRefresh(AppState& state);

// Load more results for infinite scroll
void LoadMoreResults(AppState& state);

// Filter results by file type visibility settings
void FilterResultsByFileType(AppState& state);

// Search Suggestions (implemented in main.cpp)

// Normalize text for suggestion matching
std::string NormalizeSuggestText(const std::string& s);

// Update suggestion matches based on query
void UpdateSuggestMatches(AppState& state, const std::string& queryNorm, int maxOut = 10);

// Start rebuilding suggestion index from database
void StartSuggestRebuild(AppState& state);

// Step through suggestion build (call each frame)
void StepSuggestBuild(AppState& state, int steps = 2);

// Splash Pre-upload Helpers (implemented in main.cpp)

// Pre-upload waveform textures to GPU during splash (bypasses per-frame budget).
// count: max number of items from state.searchResults/waveformCache to upload.
void PreUploadWaveformTextures(AppState& state, int count);
