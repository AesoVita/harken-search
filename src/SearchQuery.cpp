#include "SearchQuery.h"
#include "SearchTextIndex.h"
#include "Utils.h"     // Unicode query normalization.

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace {

static std::string toLowerAscii(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string escapeFtsQuoted(std::string s) {
    // FTS5 quoted strings use double quotes; escape by doubling them.
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    return out;
}

static bool isSimpleAsciiWord(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (c >= 0x80) return false;
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    return true;
}

static bool isSimpleAsciiWordWithTrailingStar(const std::string& s) {
    if (s.size() < 2 || s.back() != '*') return false;
    std::string base = s.substr(0, s.size() - 1);
    return isSimpleAsciiWord(base);
}

static bool hasAtLeastUtf8Codepoints(const std::string& s, size_t minimum) {
    size_t count = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t len = 1;
        if ((c & 0x80) == 0) {
            len = 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            len = 4;
        }
        i += len;
        if (++count >= minimum) return true;
    }
    return false;
}

static std::string normalizeTokenForMatch(const std::string& raw, bool allowPrefix) {
    if (isSimpleAsciiWordWithTrailingStar(raw)) return raw;
    if (isSimpleAsciiWord(raw)) {
        return allowPrefix ? (raw + "*") : raw;
    }

    std::string quoted = "\"" + escapeFtsQuoted(raw) + "\"";
    if (allowPrefix) quoted += "*";
    return quoted;
}

enum class TokType {
    End,
    LParen,
    RParen,
    Or,
    Not,
    Word,
    Phrase,
    Colon,
    Minus,
};

struct Tok {
    TokType type = TokType::End;
    std::string text;
};

class Lexer {
public:
    explicit Lexer(std::string input) : m_input(std::move(input)) {}

    Tok next() {
        skipWs();
        if (m_pos >= m_input.size()) return {TokType::End, ""};

        char c = m_input[m_pos];
        // Treat common tag separators as whitespace outside quotes.
        // This makes queries like "metal, hit" behave like "metal hit".
        if (c == ',' || c == ';') { m_pos++; return next(); }
        if (c == '(') { m_pos++; return {TokType::LParen, "("}; }
        if (c == ')') { m_pos++; return {TokType::RParen, ")"}; }
        if (c == ':') { m_pos++; return {TokType::Colon, ":"}; }
        if (c == '-') { m_pos++; return {TokType::Minus, "-"}; }
        if (c == '"') return readPhrase();

        return readWord();
    }

private:
    Tok readPhrase() {
        // Read until next quote, allow escaped quotes by doubling ("")
        // Input: "hello ""world""" -> hello "world"
        size_t start = m_pos;
        (void)start;
        m_pos++; // consume opening "
        std::string out;
        while (m_pos < m_input.size()) {
            char c = m_input[m_pos++];
            if (c == '"') {
                // doubled quote?
                if (m_pos < m_input.size() && m_input[m_pos] == '"') {
                    m_pos++;
                    out.push_back('"');
                    continue;
                }
                return {TokType::Phrase, out};
            }
            out.push_back(c);
        }
        // Unterminated quote
        m_error = "短语引号未闭合";
        return {TokType::Phrase, out};
    }

    Tok readWord() {
        std::string out;
        while (m_pos < m_input.size()) {
            char c = m_input[m_pos];
            // Stop at structural chars and common separators.
            // Users often paste comma-separated keywords; comma should split tokens (unless quoted).
            if (std::isspace((unsigned char)c) || c == '(' || c == ')' || c == ':' || c == '"' || c == ',' || c == ';') break;
            // keep '-' inside words (e.g. SFX-01). Unary minus is tokenized separately only when it appears as its own char.
            out.push_back(c);
            m_pos++;
        }
        std::string lower = toLowerAscii(out);
        if (lower == "or") return {TokType::Or, out};
        if (lower == "not") return {TokType::Not, out};
        return {TokType::Word, out};
    }

    void skipWs() {
        while (m_pos < m_input.size()) {
            unsigned char c = (unsigned char)m_input[m_pos];
            if (std::isspace(c) || c == ',' || c == ';') {
                m_pos++;
                continue;
            }
            break;
        }
    }

public:
    std::string m_error;

private:
    std::string m_input;
    size_t m_pos = 0;
};

struct Node {
    enum class Kind { Term, And, Or, Not, Group } kind;

    // Term
    std::string field; // empty -> all columns
    std::string value;
    bool isPhrase = false;

    // Children
    std::vector<Node> children;

    static Node term(std::string field, std::string value, bool isPhrase) {
        Node n;
        n.kind = Kind::Term;
        n.field = std::move(field);
        n.value = std::move(value);
        n.isPhrase = isPhrase;
        return n;
    }
};

static constexpr int kMaxSynonymExpansion = 8;

class Parser {
public:
    explicit Parser(std::string input, bool exactMode = false,
                    const std::vector<SynonymGroup>* synonyms = nullptr)
        : m_lex(std::move(input)), m_exactMode(exactMode), m_synonyms(synonyms) { m_cur = next(); }

    SearchQueryResult parse() {
        SearchQueryResult r;
        if (!m_lex.m_error.empty()) {
            r.ok = false;
            r.error = m_lex.m_error;
            return r;
        }

        Node root = parseExpr();
        if (!m_lex.m_error.empty() && m_err.empty()) {
            m_err = m_lex.m_error;
        }
        if (!m_err.empty()) {
            r.ok = false;
            r.error = m_err;
            return r;
        }
        if (m_cur.type != TokType::End) {
            r.ok = false;
            r.error = "查询语法错误：无法解析的内容";
            return r;
        }

        // Emit MATCH and non-FTS filters
        r.ftsMatch = emitMatch(root);
        r.pathContains = std::move(m_pathContains);
        r.tagFilters = std::move(m_tagFilters);
        r.colorFilters = std::move(m_colorFilters);

        // Supplement with trigram-backed file_name substring matches.
        // Only safe when the expression is a positive AND-conjunction — OR/NOT
        // trees would break semantics if we UNIONed substring hits.
        if (!m_exactMode && isPositiveConjunctionOnly(root)) {
            collectTrigramTerms(root, r.nameContains);
        }

        // Detect purely-negative query (e.g. "NOT hit", "NOT hit NOT speech").
        // FTS5 MATCH 'NOT term*' with no preceding positive term returns 0 rows.
        // Instead, extract the excluded terms and let Database.cpp use SQL NOT LIKE.
        if (isPurelyNegative(root)) {
            collectExcludeTerms(root, r.excludeTerms);
            r.ftsMatch = "";
        }

        return r;
    }

    static bool isPurelyNegative(const Node& n) {
        if (n.kind == Node::Kind::Not) return true;
        if (n.kind == Node::Kind::And)
            return std::all_of(n.children.begin(), n.children.end(),
                [](const Node& c) { return c.kind == Node::Kind::Not; });
        return false;
    }

    // True iff the AST is a positive AND-conjunction of Term nodes (and Groups
    // wrapping the same). Returns false for any tree containing Or or Not.
    // Used to decide whether we can safely supplement FTS with a trigram
    // substring filter without breaking OR/NOT semantics.
    static bool isPositiveConjunctionOnly(const Node& n) {
        switch (n.kind) {
        case Node::Kind::Term:
            return true;
        case Node::Kind::And:
            for (const auto& c : n.children) {
                if (!isPositiveConjunctionOnly(c)) return false;
            }
            return true;
        case Node::Kind::Group:
            if (n.children.empty()) return false;
            return isPositiveConjunctionOnly(n.children[0]);
        default: // Or, Not
            return false;
        }
    }

    // Walks the AST and collects Term values that should ALSO be matched
    // against the file_name trigram sidecar. Skips quoted phrases
    // (user explicitly scoped), field-prefixed terms, and tokens shorter
    // than three Unicode codepoints (trigram tokenizer minimum).
    static void collectTrigramTerms(const Node& n, std::vector<std::string>& out) {
        switch (n.kind) {
        case Node::Kind::Term: {
            if (n.isPhrase) return;
            if (!n.field.empty()) return;
            if (!hasAtLeastUtf8Codepoints(n.value, 3)) return;
            std::string initials;
            if (HarkenSearch::Search::DeriveInitialsFromKnownPinyinToken(n.value, initials)) return;
            out.push_back(n.value);
            return;
        }
        case Node::Kind::And:
        case Node::Kind::Group:
            for (const auto& c : n.children) collectTrigramTerms(c, out);
            return;
        default: // Or, Not
            return;
        }
    }

    static void collectExcludeTerms(const Node& n, std::vector<std::string>& out) {
        if (n.kind == Node::Kind::Not && !n.children.empty()) {
            const Node& inner = n.children[0];
            if (inner.kind == Node::Kind::Term && !inner.value.empty())
                out.push_back(inner.value);
        } else if (n.kind == Node::Kind::And) {
            for (const auto& c : n.children) collectExcludeTerms(c, out);
        }
    }

private:
    Tok next() {
        if (m_hasPeeked) {
            m_hasPeeked = false;
            return m_peeked;
        }
        return m_lex.next();
    }

    const Tok& peek() {
        if (!m_hasPeeked) {
            m_peeked = m_lex.next();
            m_hasPeeked = true;
        }
        return m_peeked;
    }

    void advance() { m_cur = next(); }

    bool isStartOfPrimary(const Tok& t) const {
        return t.type == TokType::Word || t.type == TokType::Phrase || t.type == TokType::LParen || t.type == TokType::Minus || t.type == TokType::Not;
    }

    Node parseExpr() { return parseOr(); }

    Node parseOr() {
        Node left = parseAnd();
        if (!m_err.empty()) return left;

        std::vector<Node> parts;
        parts.push_back(std::move(left));
        while (m_cur.type == TokType::Or) {
            advance();
            parts.push_back(parseAnd());
            if (!m_err.empty()) return parts.front();
        }
        if (parts.size() == 1) return parts.front();
        Node n;
        n.kind = Node::Kind::Or;
        n.children = std::move(parts);
        return n;
    }

    Node parseAnd() {
        Node first = parseUnary();
        if (!m_err.empty()) return first;

        std::vector<Node> parts;
        parts.push_back(std::move(first));
        while (isStartOfPrimary(m_cur) && m_cur.type != TokType::Or && m_cur.type != TokType::RParen && m_cur.type != TokType::End) {
            parts.push_back(parseUnary());
            if (!m_err.empty()) return parts.front();
        }
        // Implicit AND: adjacent terms/groups.
        if (parts.size() == 1) return parts.front();
        Node n;
        n.kind = Node::Kind::And;
        n.children = std::move(parts);
        return n;
    }

    Node parseUnary() {
        if (m_cur.type == TokType::Minus || m_cur.type == TokType::Not) {
            advance();
            Node inner = parseUnary();
            Node n;
            n.kind = Node::Kind::Not;
            n.children.push_back(std::move(inner));
            return n;
        }
        return parsePrimary();
    }

    Node parsePrimary() {
        if (m_cur.type == TokType::LParen) {
            advance();
            Node inside = parseExpr();
            if (m_cur.type != TokType::RParen) {
                m_err = "括号未闭合";
                return inside;
            }
            advance();
            Node g;
            g.kind = Node::Kind::Group;
            g.children.push_back(std::move(inside));
            return g;
        }

        // term or field:term
        if (m_cur.type == TokType::Word) {
            std::string maybeField = m_cur.text;
            if (peek().type == TokType::Colon) {
                advance(); // consume field word
                advance(); // consume ':'
                return parseFieldTerm(toLowerAscii(maybeField));
            }
        }

        if (m_cur.type == TokType::Word || m_cur.type == TokType::Phrase) {
            bool isPhrase = (m_cur.type == TokType::Phrase);
            std::string v = m_cur.text;
            advance();
            return Node::term("", std::move(v), isPhrase);
        }

        m_err = "查询语法错误：缺少搜索词";
        Node dummy = Node::term("", "", false);
        return dummy;
    }

    Node parseFieldTerm(const std::string& field) {
        if (m_cur.type != TokType::Word && m_cur.type != TokType::Phrase) {
            m_err = "字段后缺少搜索词";
            return Node::term("", "", false);
        }
        bool isPhrase = (m_cur.type == TokType::Phrase);
        std::string v = m_cur.text;
        advance();

        // Field mapping / extraction (non-FTS fields extracted separately)
        if (field == "path") {
            if (!v.empty()) m_pathContains.push_back(v);
            return Node::term("", "", false);
        }
        if (field == "tag" || field == "tags") {
            if (!v.empty()) m_tagFilters.push_back(v);
            return Node::term("", "", false);
        }
        if (field == "color" || field == "colour") {
            if (!v.empty()) m_colorFilters.push_back(v);
            return Node::term("", "", false);
        }
        std::string mapped;
        if (field == "artist") mapped = "artist";
        else if (field == "album") mapped = "album";
        else if (field == "title") mapped = "title";
        else if (field == "category") mapped = "category";
        else if (field == "keywords") mapped = "keywords";
        else if (field == "name") mapped = "file_name";
        else if (field == "desc" || field == "description") mapped = "description";
        else mapped.clear(); // unknown -> treat as global term

        return Node::term(mapped, std::move(v), isPhrase);
    }

    std::vector<std::string> lookupSynonyms(const std::string& token) const {
        if (!m_synonyms) return {};
        std::string lower = toLowerAscii(token);
        // Bidirectional lookup: a token may live in either `triggers` or `related`.
        // Hit on triggers  -> expand to related.
        // Hit on related   -> expand to triggers + other related (excluding self).
        for (const auto& group : *m_synonyms) {
            bool inTriggers = false, inRelated = false;
            for (const auto& t : group.triggers) {
                if (toLowerAscii(t) == lower) { inTriggers = true; break; }
            }
            if (!inTriggers) {
                for (const auto& r : group.related) {
                    if (toLowerAscii(r) == lower) { inRelated = true; break; }
                }
            }
            if (!inTriggers && !inRelated) continue;

            std::vector<std::string> result;
            auto pushUnique = [&](const std::string& w) -> bool {
                if (toLowerAscii(w) == lower) return true;  // skip self, keep going
                for (const auto& existing : result) {
                    if (toLowerAscii(existing) == toLowerAscii(w)) return true;  // dedupe
                }
                result.push_back(w);
                return (int)result.size() < kMaxSynonymExpansion;
            };

            if (inTriggers) {
                for (const auto& w : group.related) {
                    if (!pushUnique(w)) return result;
                }
            } else {
                for (const auto& w : group.triggers) {
                    if (!pushUnique(w)) return result;
                }
                for (const auto& w : group.related) {
                    if (!pushUnique(w)) return result;
                }
            }
            return result;  // first matching group wins; avoids cross-group expansion explosion
        }
        return {};
    }

    std::string emitMatch(const Node& n) {
        switch (n.kind) {
        case Node::Kind::Term: {
            if (n.value.empty()) return "";
            std::string tok;
            if (n.isPhrase) {
                tok = "\"" + escapeFtsQuoted(n.value) + "\"";
            } else {
                tok = normalizeTokenForMatch(n.value, /*allowPrefix*/!m_exactMode);
            }
            if (!n.field.empty()) return n.field + ":" + tok;

            if (!n.isPhrase && !m_exactMode) {
                std::string initials;
                if (HarkenSearch::Search::DeriveInitialsFromKnownPinyinToken(n.value, initials)) {
                    return "(" + tok + " OR " + normalizeTokenForMatch(initials, true) + ")";
                }

                auto syns = lookupSynonyms(n.value);
                if (!syns.empty()) {
                    std::string expanded = "(" + tok;
                    for (const auto& syn : syns) {
                        expanded += " OR " + normalizeTokenForMatch(syn, true);
                    }
                    expanded += ")";
                    return expanded;
                }
            }

            return tok;
        }
        case Node::Kind::Not: {
            std::string inner = emitMatch(n.children.empty() ? Node::term("", "", false) : n.children[0]);
            if (inner.empty()) return "";
            return "NOT " + maybeGroup(inner);
        }
        case Node::Kind::And: {
            std::vector<std::string> positives;
            std::vector<std::string> negatives;
            for (const auto& c : n.children) {
                std::string part = emitMatch(c);
                if (part.empty()) continue;
                if (c.kind == Node::Kind::Not) {
                    negatives.push_back(std::move(part));
                } else {
                    positives.push_back(maybeGroup(part));
                }
            }
            std::string out;
            if (!positives.empty()) {
                out = positives[0];
                for (size_t i = 1; i < positives.size(); ++i) out += " AND " + positives[i];
            }
            for (const auto& negative : negatives) {
                if (!out.empty()) out += " ";
                out += negative;
            }
            return out;
        }
        case Node::Kind::Or: {
            std::vector<std::string> parts;
            for (const auto& c : n.children) {
                std::string part = emitMatch(c);
                if (!part.empty()) parts.push_back(maybeGroup(part));
            }
            if (parts.empty()) return "";
            if (parts.size() == 1) return parts[0];
            std::string out = parts[0];
            for (size_t i = 1; i < parts.size(); i++) out += " OR " + parts[i];
            return out;
        }
        case Node::Kind::Group: {
            if (n.children.empty()) return "";
            std::string inner = emitMatch(n.children[0]);
            if (inner.empty()) return "";
            return "(" + inner + ")";
        }
        default:
            return "";
        }
    }

    static std::string maybeGroup(const std::string& s) {
        // Keep already-grouped expressions intact.
        // For NOT we already prefix.
        if (s.empty()) return s;
        if (s.front() == '(' && s.back() == ')') return s;
        // If it contains spaces or OR/NOT, group it.
        if (s.find(' ') != std::string::npos) return "(" + s + ")";
        return s;
    }

private:
    Lexer m_lex;
    Tok m_cur;
    std::string m_err;
    std::vector<std::string> m_pathContains;
    std::vector<std::string> m_tagFilters;
    std::vector<std::string> m_colorFilters;
    bool m_exactMode = false;
    const std::vector<SynonymGroup>* m_synonyms = nullptr;

    Tok m_peeked;
    bool m_hasPeeked = false;
};

// Pre-expand whole-word synonym matches BEFORE InjectCJKSpaces.
// This is required because InjectCJKSpaces splits multi-CJK-char words (e.g. "啦啦啦")
// into single-char tokens, which prevents the parser from matching multi-char dict
// entries. By expanding at word level here, we replace matched tokens with a
// parenthesized OR group; the CJK splitter then runs harmlessly on each side.
//
// Bidirectional: hits on either `triggers` or `related` are honored.
// Skips: text inside quoted phrases, words after `:` (field values), OR/NOT keywords.
static std::string PreExpandSynonyms(const std::string& input,
                                     const std::vector<SynonymGroup>& dict) {
    std::string out;
    out.reserve(input.size() * 2);
    size_t i = 0;
    bool prevWasColon = false;

    auto isDelim = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == ',' || c == ';' ||
               c == '(' || c == ')' || c == ':' || c == '"';
    };

    while (i < input.size()) {
        unsigned char c = (unsigned char)input[i];
        if (c == '"') {
            // Copy quoted phrase verbatim so user's exact-phrase intent is preserved.
            out.push_back((char)c);
            i++;
            while (i < input.size()) {
                if (input[i] == '"') {
                    if (i + 1 < input.size() && input[i + 1] == '"') {
                        out.push_back('"'); out.push_back('"'); i += 2; continue;
                    }
                    out.push_back('"'); i++;
                    break;
                }
                out.push_back(input[i++]);
            }
            prevWasColon = false;
            continue;
        }
        if (isDelim(c) && c != ':') {
            out.push_back((char)c);
            i++;
            prevWasColon = false;
            continue;
        }
        if (c == ':') {
            out.push_back((char)c);
            i++;
            prevWasColon = true;
            continue;
        }
        // Read word (any non-delim run).
        size_t start = i;
        while (i < input.size() && !isDelim((unsigned char)input[i])) i++;
        std::string word = input.substr(start, i - start);
        if (word.empty()) continue;

        if (prevWasColon) {
            out += word;
            prevWasColon = false;
            continue;
        }

        std::string lower = toLowerAscii(word);
        if (lower == "or" || lower == "not") {
            out += word;
            continue;
        }

        // Bidirectional lookup, first matching group wins.
        std::vector<std::string> expansions;
        for (const auto& group : dict) {
            bool inT = false, inR = false;
            for (const auto& t : group.triggers) {
                if (toLowerAscii(t) == lower) { inT = true; break; }
            }
            if (!inT) {
                for (const auto& r2 : group.related) {
                    if (toLowerAscii(r2) == lower) { inR = true; break; }
                }
            }
            if (!inT && !inR) continue;

            auto pushUnique = [&](const std::string& w) -> bool {
                if (toLowerAscii(w) == lower) return true;
                for (const auto& e : expansions) {
                    if (toLowerAscii(e) == toLowerAscii(w)) return true;
                }
                expansions.push_back(w);
                return (int)expansions.size() < kMaxSynonymExpansion;
            };
            if (inT) {
                for (const auto& w : group.related) if (!pushUnique(w)) break;
            } else {
                for (const auto& w : group.triggers) if (!pushUnique(w)) break;
                for (const auto& w : group.related)  if (!pushUnique(w)) break;
            }
            break;
        }

        if (expansions.empty()) {
            out += word;
        } else {
            out += '(';
            out += word;
            for (const auto& e : expansions) { out += " OR "; out += e; }
            out += ')';
        }
    }
    return out;
}

} // namespace

SearchQueryResult ParseSearchQueryToFts5(const std::string& input, bool exactMode,
                                         const std::vector<SynonymGroup>* synonyms) {
    SearchQueryResult r;
    if (input.empty()) return r;

    bool willPreExpand = synonyms && !synonyms->empty() && !exactMode;

    // Parse without synonym expansion to preserve nameContains for the trigram
    // substring path. Expansion wraps a Term in Group(Or(...)), which would
    // make isPositiveConjunctionOnly() reject the AST and lose matches such
    // as "step" in "footstep.wav".

    std::vector<std::string> baseNameContains;
    if (willPreExpand) {
        std::string baseInput = InjectCJKSpaces(input);
        Parser baseParser(baseInput, exactMode, /*synonyms*/nullptr);
        SearchQueryResult baseResult = baseParser.parse();
        baseNameContains = std::move(baseResult.nameContains);
    }

    // Parse the expanded query for FTS matching so multi-character CJK
    // synonyms survive per-character splitting.
    std::string preprocessed = willPreExpand
        ? PreExpandSynonyms(input, *synonyms)
        : input;

    std::string processedInput = InjectCJKSpaces(preprocessed);

    // Pass nullptr so the per-token lookup doesn't double-expand on top of
    // PreExpandSynonyms; the bidirectional Parser::lookupSynonyms still
    // exists as a defensive fallback for any caller that bypasses pre-expansion.
    Parser p(processedInput, exactMode, /*synonyms*/nullptr);
    r = p.parse();

    // Overlay the original-input trigram terms so the UNION (FTS ∪ trigram)
    // query path still fires for synonym-expanded queries.
    if (willPreExpand && !baseNameContains.empty()) {
        r.nameContains = std::move(baseNameContains);
    }

    return r;
}

static std::vector<std::string> TokenizeSearchChipText(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;
    for (char c : input) {
        if (c == '"') {
            inQuote = !inQuote;
            token.push_back(c);
            continue;
        }
        if (std::isspace((unsigned char)c) && !inQuote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(c);
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

static std::string StripSearchChipQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

static bool SplitSearchChipField(const std::string& token,
                                 std::string& field,
                                 std::string& value) {
    size_t colon = token.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= token.size()) return false;
    std::string candidate = token.substr(0, colon);
    for (unsigned char c : candidate) {
        if (!(std::isalnum(c) || c == '_')) return false;
    }
    field = toLowerAscii(candidate);
    value = token.substr(colon + 1);
    return true;
}

static SearchQueryChip BuildOneSearchQueryChip(const std::string& rawToken,
                                               bool excluded,
                                               std::vector<std::string> sourceTokens) {
    SearchQueryChip chip;
    chip.sourceTokens = std::move(sourceTokens);

    std::string token = rawToken;
    if (!token.empty() && token.front() == '-') {
        token.erase(token.begin());
        excluded = true;
    }

    std::string field;
    std::string value;
    if (SplitSearchChipField(token, field, value)) {
        chip.kind = excluded ? SearchQueryChipKind::ExcludeField : SearchQueryChipKind::Field;
        chip.field = std::move(field);
        chip.label = StripSearchChipQuotes(value);
    } else {
        chip.kind = excluded ? SearchQueryChipKind::ExcludeKeyword : SearchQueryChipKind::Keyword;
        chip.label = StripSearchChipQuotes(token);
    }
    return chip;
}

std::vector<SearchQueryChip> BuildSearchQueryChips(const std::string& input) {
    std::vector<SearchQueryChip> chips;
    std::vector<std::string> tokens = TokenizeSearchChipText(input);
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].empty()) continue;
        std::string lower = toLowerAscii(tokens[i]);
        if (lower == "or") {
            SearchQueryChip chip;
            chip.kind = SearchQueryChipKind::Operator;
            chip.label = tokens[i];
            chip.sourceTokens = std::vector<std::string>{tokens[i]};
            chips.push_back(std::move(chip));
            continue;
        }
        if (lower == "not") {
            if (i + 1 < tokens.size()) {
                chips.push_back(BuildOneSearchQueryChip(
                    tokens[i + 1],
                    true,
                    std::vector<std::string>{tokens[i], tokens[i + 1]}));
                ++i;
                continue;
            }
            SearchQueryChip chip;
            chip.kind = SearchQueryChipKind::Operator;
            chip.label = tokens[i];
            chip.sourceTokens = std::vector<std::string>{tokens[i]};
            chips.push_back(std::move(chip));
            continue;
        }
        const bool minusExcluded = tokens[i].size() > 1 && tokens[i].front() == '-';
        chips.push_back(BuildOneSearchQueryChip(tokens[i], minusExcluded, std::vector<std::string>{tokens[i]}));
    }
    return chips;
}

std::string RemoveSearchQueryChipFromText(const std::string& input, const SearchQueryChip& chip) {
    std::vector<std::string> tokens = TokenizeSearchChipText(input);
    if (tokens.empty() || chip.sourceTokens.empty()) return input;

    auto matchesAt = [&](size_t start) {
        if (start + chip.sourceTokens.size() > tokens.size()) return false;
        for (size_t i = 0; i < chip.sourceTokens.size(); ++i) {
            if (toLowerAscii(tokens[start + i]) != toLowerAscii(chip.sourceTokens[i])) {
                return false;
            }
        }
        return true;
    };

    std::vector<std::string> kept;
    kept.reserve(tokens.size());
    bool removed = false;
    for (size_t i = 0; i < tokens.size();) {
        if (!removed && matchesAt(i)) {
            i += chip.sourceTokens.size();
            removed = true;
            continue;
        }
        kept.push_back(tokens[i++]);
    }

    std::string out;
    for (const auto& token : kept) {
        if (!out.empty()) out += ' ';
        out += token;
    }
    return out;
}

