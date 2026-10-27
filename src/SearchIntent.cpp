#include "SearchIntent.h"

#include <cctype>
#include <string>
#include <vector>

namespace HarkenSearch::SearchIntent {
namespace {

struct Token {
    std::string text;
    bool phrase = false;
};

static std::string trimCopy(std::string_view in) {
    size_t first = 0;
    while (first < in.size() && std::isspace(static_cast<unsigned char>(in[first]))) ++first;
    size_t last = in.size();
    while (last > first && std::isspace(static_cast<unsigned char>(in[last - 1]))) --last;
    return std::string(in.substr(first, last - first));
}

static std::string lowerAscii(std::string_view in) {
    std::string out(in);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

static bool tokenize(std::string_view input, std::vector<Token>& out, std::string& error) {
    out.clear();
    size_t pos = 0;
    while (pos < input.size()) {
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) ++pos;
        if (pos >= input.size()) break;

        if (input[pos] == '"') {
            ++pos;
            Token token;
            token.phrase = true;
            while (pos < input.size()) {
                char c = input[pos++];
                if (c == '"') {
                    if (pos < input.size() && input[pos] == '"') {
                        ++pos;
                        token.text.push_back('"');
                        continue;
                    }
                    out.push_back(std::move(token));
                    token.phrase = false;
                    break;
                }
                token.text.push_back(c);
            }
            if (token.phrase) {
                error = "Advanced search: unterminated quoted value";
                return false;
            }
            continue;
        }

        Token token;
        while (pos < input.size() && !std::isspace(static_cast<unsigned char>(input[pos]))) {
            token.text.push_back(input[pos++]);
        }
        out.push_back(std::move(token));
    }
    return true;
}

static bool isSupportedField(std::string_view field) {
    const std::string f = lowerAscii(field);
    return f == "name" ||
           f == "path" ||
           f == "category" ||
           f == "artist" ||
           f == "album" ||
           f == "keywords" ||
           f == "desc" ||
           f == "tag" ||
           f == "color";
}

static bool isConjunctiveSideFilter(std::string_view field) {
    const std::string f = lowerAscii(field);
    return f == "path" || f == "tag" || f == "color";
}

static bool parseOperator(std::string_view op, SearchClauseOperator& parsed, bool& negated) {
    const std::string o = lowerAscii(op);
    negated = false;
    if (o == "contains") {
        parsed = SearchClauseOperator::Contains;
        return true;
    }
    if (o == "=" || o == "equals" || o == "is") {
        parsed = SearchClauseOperator::Equals;
        return true;
    }
    if (o == "excludes" || o == "!=") {
        parsed = SearchClauseOperator::Excludes;
        negated = true;
        return true;
    }
    return false;
}

static bool isOperatorToken(std::string_view op) {
    SearchClauseOperator ignored = SearchClauseOperator::Contains;
    bool negated = false;
    return parseOperator(op, ignored, negated);
}

static std::string quoteValue(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::string joinTokens(const std::vector<Token>& tokens, size_t first, size_t lastExclusive) {
    std::string out;
    for (size_t i = first; i < lastExclusive; ++i) {
        if (!out.empty()) out.push_back(' ');
        out += tokens[i].text;
    }
    return out;
}

static std::string freeTokenToKeyword(const Token& token) {
    return token.phrase ? quoteValue(token.text) : token.text;
}

static std::string joinFreeTokens(const std::vector<Token>& tokens, SearchIntentMatchMode mode) {
    if (tokens.empty()) return "";
    if (mode == SearchIntentMatchMode::Any && tokens.size() > 1) {
        std::string out = "(";
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) out += " OR ";
            out += freeTokenToKeyword(tokens[i]);
        }
        out.push_back(')');
        return out;
    }

    std::string out;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) out.push_back(' ');
        out += freeTokenToKeyword(tokens[i]);
    }
    return out;
}

static std::string clauseToKeyword(const SearchClause& clause) {
    std::string out;
    if (clause.negated || clause.op == SearchClauseOperator::Excludes) out.push_back('-');
    out += lowerAscii(clause.field);
    out.push_back(':');
    out += quoteValue(clause.value);
    return out;
}

static bool startsWithKnownSlashDirective(std::string_view input) {
    const std::string trimmed = trimCopy(input);
    if (trimmed.empty()) return false;
    size_t end = 0;
    while (end < trimmed.size() && !std::isspace(static_cast<unsigned char>(trimmed[end]))) ++end;
    const std::string first = lowerAscii(std::string_view(trimmed).substr(0, end));
    return first == "/all" || first == "/any" || first == "/where" || first == "/not";
}

static bool parseClauseAt(const std::vector<Token>& tokens,
                          size_t& pos,
                          bool forceNegated,
                          bool consumeRemainingValue,
                          SearchClause& clause,
                          std::string& error)
{
    if (pos >= tokens.size()) {
        error = "Advanced search: missing field";
        return false;
    }
    const std::string field = lowerAscii(tokens[pos].text);
    if (!isSupportedField(field)) {
        error = "Advanced search: unsupported field '" + tokens[pos].text + "'";
        return false;
    }
    ++pos;

    if (pos >= tokens.size()) {
        error = "Advanced search: missing operator after field '" + field + "'";
        return false;
    }
    SearchClauseOperator op = SearchClauseOperator::Contains;
    bool opNegated = false;
    if (!parseOperator(tokens[pos].text, op, opNegated)) {
        error = "Advanced search: unsupported operator '" + tokens[pos].text + "'";
        return false;
    }
    ++pos;

    if (pos >= tokens.size()) {
        error = "Advanced search: missing value after field '" + field + "'";
        return false;
    }

    const size_t valueEnd = consumeRemainingValue ? tokens.size() : pos + 1;
    clause.field = field;
    clause.op = op;
    clause.value = joinTokens(tokens, pos, valueEnd);
    clause.negated = forceNegated || opNegated;
    pos = valueEnd;
    return true;
}

} // namespace

SearchIntentCompileResult CompileSearchIntentToKeyword(const SearchIntent& intent) {
    std::vector<std::string> parts;
    if (!trimCopy(intent.freeText).empty()) {
        parts.push_back(trimCopy(intent.freeText));
    }
    for (const SearchClause& clause : intent.clauses) {
        if (clause.field.empty() || clause.value.empty()) {
            return {false, "", "Advanced search: clause must include field and value"};
        }
        if (!isSupportedField(clause.field)) {
            return {false, "", "Advanced search: unsupported field '" + clause.field + "'"};
        }
        if (intent.mode == SearchIntentMatchMode::Any && isConjunctiveSideFilter(clause.field)) {
            return {false, "", "Advanced search: /any does not support path/tag/color in v1; use /all"};
        }
        parts.push_back(clauseToKeyword(clause));
    }
    if (parts.empty()) return {true, "", ""};

    std::string keyword;
    if (intent.mode == SearchIntentMatchMode::Any && parts.size() > 1) {
        keyword.push_back('(');
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) keyword += " OR ";
            keyword += parts[i];
        }
        keyword.push_back(')');
    } else {
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) keyword.push_back(' ');
            keyword += parts[i];
        }
    }
    return {true, keyword, ""};
}

SearchIntentCompileResult ParseSlashSearchIntent(std::string_view input) {
    std::vector<Token> tokens;
    std::string error;
    if (!tokenize(trimCopy(input), tokens, error)) return {false, "", error};
    if (tokens.empty()) return {true, "", ""};

    const std::string directive = lowerAscii(tokens[0].text);
    if (directive != "/all" && directive != "/any" && directive != "/where" && directive != "/not") {
        return {false, "", "Advanced search: unsupported slash command '" + tokens[0].text + "'"};
    }

    SearchIntent intent;
    size_t pos = 1;
    if (directive == "/any") intent.mode = SearchIntentMatchMode::Any;

    if (directive == "/where" || directive == "/not") {
        SearchClause clause;
        if (!parseClauseAt(tokens, pos, directive == "/not", true, clause, error)) {
            return {false, "", error};
        }
        intent.clauses.push_back(std::move(clause));
        return CompileSearchIntentToKeyword(intent);
    }

    std::vector<Token> freeTokens;
    while (pos < tokens.size()) {
        bool negated = false;
        if (lowerAscii(tokens[pos].text) == "not") {
            negated = true;
            ++pos;
            if (pos >= tokens.size()) {
                freeTokens.push_back({"not", false});
                break;
            }
        }

        if (pos + 2 < tokens.size() && isOperatorToken(tokens[pos + 1].text)) {
            SearchClause clause;
            if (!parseClauseAt(tokens, pos, negated, false, clause, error)) {
                return {false, "", error};
            }
            intent.clauses.push_back(std::move(clause));
        } else {
            if (negated) freeTokens.push_back({"not", false});
            freeTokens.push_back(tokens[pos++]);
        }
    }

    intent.freeText = joinFreeTokens(freeTokens, intent.mode);
    return CompileSearchIntentToKeyword(intent);
}

SearchIntentCompileResult CompileSlashSearchInputToKeyword(std::string_view input) {
    if (!startsWithKnownSlashDirective(input)) {
        return {true, std::string(input), ""};
    }
    return ParseSlashSearchIntent(input);
}

} // namespace HarkenSearch::SearchIntent
