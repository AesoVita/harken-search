#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace SearchCommands {

// @stage7-contract
// API contract: the command registry is process-owned immutable storage;
// GetAllCommands and MatchCommandsByPrefix expose non-owning references or
// pointers valid for process lifetime. Parsing/quoting helpers borrow input
// views for each synchronous call and return owned values where applicable.

enum class CmdType { Operator, Field };

enum class CmdValueSource { None, Categories, PathFacets };

struct SearchCommand {
    std::string_view name;           // e.g. "or", "category"
    CmdType          type;
    std::string_view insertTemplate; // text inserted into searchBuffer
    CmdValueSource   valueSource;
    std::string_view description;    // Chinese label shown in menu
    std::string_view syntaxHint;     // right-aligned hint, e.g. "a OR b"
};

const std::vector<SearchCommand>& GetAllCommands();

// Returns pointers into GetAllCommands() whose name starts with `prefix`
// (prefix must NOT include the leading '/').
// Empty prefix returns all commands.
std::vector<const SearchCommand*> MatchCommandsByPrefix(std::string_view prefix);

// Returns true if slashPos in buffer is a valid trigger position:
//   - at buffer start (slashPos == 0), OR
//   - preceded by a whitespace character
// AND the slash is not inside an unclosed double-quote string.
// AND the character at slashPos is actually '/'.
bool IsTokenBoundary(std::string_view buffer, size_t slashPos);

// Formats a picked field value for insertion after a field prefix such as
// `category:` or `path:`. SearchQuery parses doubled quotes inside phrases.
std::string QuoteFieldValueForInsert(std::string_view value);

} // namespace SearchCommands
