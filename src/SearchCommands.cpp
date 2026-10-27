#include "SearchCommands.h"
#include <cassert>

namespace SearchCommands {

const std::vector<SearchCommand>& GetAllCommands() {
    static const std::vector<SearchCommand> kCommands = {
        { "all",      CmdType::Operator, "/all ",     CmdValueSource::None,       "Advanced ALL",         "/all ..."        },
        { "any",      CmdType::Operator, "/any ",     CmdValueSource::None,       "Advanced ANY",         "/any ..."        },
        { "where",    CmdType::Operator, "/where ",   CmdValueSource::None,       "Advanced field",       "field = value"   },
        { "or",       CmdType::Operator, " OR ",      CmdValueSource::None,       "OR operator",          "a OR b"          },
        { "not",      CmdType::Operator, " -",        CmdValueSource::None,       "Exclude term",         "-noise"          },
        { "exact",    CmdType::Operator, R"("")",     CmdValueSource::None,       "Exact phrase",         R"("foot step")" },
        { "group",    CmdType::Operator, "( )",       CmdValueSource::None,       "Group logic",          "( a OR b )"      },
        { "category", CmdType::Field,    "category:", CmdValueSource::Categories, "Filter category",      R"(category:"...")" },
        { "path",     CmdType::Field,    "path:",     CmdValueSource::PathFacets, "Filter path",          R"(path:"...")"     },
    };
    return kCommands;
}

std::vector<const SearchCommand*> MatchCommandsByPrefix(std::string_view prefix) {
    assert(prefix.empty() || prefix[0] != '/');  // caller must strip the leading '/'
    const auto& all = GetAllCommands();
    std::vector<const SearchCommand*> result;
    result.reserve(all.size());
    for (const SearchCommand& cmd : all) {
        if (cmd.name.size() >= prefix.size() &&
            cmd.name.substr(0, prefix.size()) == prefix)
        {
            result.push_back(&cmd);
        }
    }
    return result;
}

bool IsTokenBoundary(std::string_view buffer, size_t slashPos) {
    // Rule 1: character at slashPos must be '/'
    if (slashPos >= buffer.size() || buffer[slashPos] != '/') {
        return false;
    }

    // Rule 2: must be at start or preceded by ASCII whitespace.
    // Intentionally only ' ' and '\t'; std::isspace requires (unsigned char) cast
    // to avoid UB on high-byte UTF-8 continuation bytes in the search buffer.
    if (slashPos != 0) {
        const char prev = buffer[slashPos - 1];
        if (prev != ' ' && prev != '\t') {
            return false;
        }
    }

    // Rule 3: must not be inside an unclosed double-quoted string.
    // Count unescaped '"' characters in buffer[0..slashPos).
    int quoteCount = 0;
    for (size_t i = 0; i < slashPos; ++i) {
        if (buffer[i] == '"') {
            // Check for backslash escape
            int backslashes = 0;
            size_t j = i;
            while (j > 0 && buffer[j - 1] == '\\') {
                ++backslashes;
                --j;
            }
            if (backslashes % 2 == 0) {
                ++quoteCount;
            }
        }
    }
    if (quoteCount % 2 != 0) {
        return false; // inside a quoted string
    }

    return true;
}

std::string QuoteFieldValueForInsert(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 3);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    out.push_back(' ');
    return out;
}

} // namespace SearchCommands
