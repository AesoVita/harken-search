#pragma once

#include <string>
#include <string_view>
#include <vector>

// @stage7-contract
// API contract: search-intent compilation/parsing is synchronous and pure.
// Input views and intent fields are borrowed only for the call; returned
// keyword/error strings are owned by the result. Invalid syntax or clauses
// produce ok=false without global state or retained references.

namespace HarkenSearch::SearchIntent {

enum class SearchIntentMatchMode {
    All,
    Any
};

enum class SearchClauseOperator {
    Contains,
    Equals,
    Excludes
};

struct SearchClause {
    std::string field;
    SearchClauseOperator op = SearchClauseOperator::Contains;
    std::string value;
    bool negated = false;
};

struct SearchIntent {
    SearchIntentMatchMode mode = SearchIntentMatchMode::All;
    std::vector<SearchClause> clauses;
    std::string freeText;
};

struct SearchIntentCompileResult {
    bool ok = false;
    std::string keyword;
    std::string error;
};

SearchIntentCompileResult CompileSearchIntentToKeyword(const SearchIntent& intent);
SearchIntentCompileResult ParseSlashSearchIntent(std::string_view input);
SearchIntentCompileResult CompileSlashSearchInputToKeyword(std::string_view input);

} // namespace HarkenSearch::SearchIntent
