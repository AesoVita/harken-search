// Aris - Send to Wwise (F-034 S4) - Wildcards parser + structure expander.

#include "WwiseStructureTemplate.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string_view>
#include <unordered_set>

namespace aura {
namespace wwise {

namespace {

// Identifier chars allowed inside a `$name` wildcard. Digits allowed (e.g.
// future `$md5`) but leading char must be alpha — enforced by the parser
// already requiring `$` followed by a letter.
bool isIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

// Known wildcard names. Longest-match used when scanner sees an identifier
// run that could match multiple prefixes (e.g. "ucs_cat_X" → "ucs_cat").
// Kept in sync with the resolver switch in resolve().
const std::unordered_set<std::string>& knownWildcards() {
    static const std::unordered_set<std::string> k = {
        // Nullary (plain $name)
        "filename_base", "filename_full",
        "ucs_cat", "ucs_subcat", "ucs_catid",
        "library", "designer", "composer",
        "description_first", "description_slug",
        "index", "count",
        // Unary / variadic ($name(...))
        "envvar", "user", "wildcard",
        "upper", "lower", "capitalize",
    };
    return k;
}

std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string capitalize(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    for (std::size_t i = 1; i < s.size(); ++i) {
        s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return s;
}

// First whitespace-separated token of a string. Used for $description_first.
std::string firstWord(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    std::size_t j = i;
    while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
    return s.substr(i, j - i);
}

// Lowercase + collapse runs of non-alnum to single underscore, trim ends.
// Used for $description_slug. Keeps ASCII only; non-ASCII bytes pass through
// as underscores (cheap and predictable; internationalised slugs are out of
// scope for v0).
std::string slugify(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool lastWasUnderscore = true; // suppress leading underscore
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out.push_back(static_cast<char>(c));
            lastWasUnderscore = false;
        } else if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c + ('a' - 'A')));
            lastWasUnderscore = false;
        } else {
            if (!lastWasUnderscore) {
                out.push_back('_');
                lastWasUnderscore = true;
            }
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

// Zero-pad `value` to enough digits to represent `width` (=count), keeping all
// $index values aligned within one batch in Wwise's Project Explorer.
std::string formatIndex(int value, int countHint) {
    int width = 1;
    int n = countHint > 0 ? countHint : value;
    while (n >= 10) { n /= 10; ++width; }
    std::string raw = std::to_string(value);
    if (static_cast<int>(raw.size()) >= width) return raw;
    return std::string(width - static_cast<int>(raw.size()), '0') + raw;
}

// Resolution outcome for a single wildcard invocation. `resolved=false` means
// the wildcard name is known but the lookup produced nothing (empty value
// from a potentially-empty source field is still treated as resolved — the
// user explicitly asked for that source).
struct Resolved {
    bool resolved = true;
    std::string value;
};

// Forward: parser needs this, resolver needs parser — declare both.
ExpandResult expandImpl(std::string_view tmpl, const MetadataContext& ctx);

Resolved resolve(const std::string& name,
                 const std::vector<std::string>& args,
                 const MetadataContext& ctx) {
    if (name == "filename_base")      return {true, ctx.filenameBase};
    if (name == "filename_full")      return {true, ctx.filenameFull};
    if (name == "ucs_cat")            return {true, ctx.ucsCategory};
    if (name == "ucs_subcat")         return {true, ctx.ucsSubCategory};
    if (name == "ucs_catid")          return {true, ctx.ucsCatId};
    if (name == "library")            return {true, ctx.library};
    if (name == "designer")           return {true, ctx.designer};
    if (name == "composer")           return {true, ctx.composer};
    if (name == "description_first")  return {true, firstWord(ctx.description)};
    if (name == "description_slug")   return {true, slugify(ctx.description)};
    if (name == "index")              return {true, formatIndex(ctx.index, ctx.count)};
    if (name == "count")              return {true, std::to_string(ctx.count)};

    if (name == "envvar") {
        if (args.size() != 1) return {false, ""};
        // std::getenv is thread-safe enough for our purposes (env is not
        // mutated at runtime) and portable across MSVC/GCC/Clang; the MSVC
        // deprecation warning is a false alarm in this read-only context.
#if defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable: 4996)
#endif
        const char* v = std::getenv(args[0].c_str());
#if defined(_MSC_VER)
        #pragma warning(pop)
#endif
        if (v == nullptr) return {false, ""};
        return {true, std::string(v)};
    }
    if (name == "user") {
        if (args.size() != 1) return {false, ""};
        auto it = ctx.userVars.find(args[0]);
        if (it == ctx.userVars.end()) return {false, ""};
        return {true, it->second};
    }
    if (name == "wildcard") {
        // First non-empty wins. All args already expanded by the parser.
        for (const auto& a : args) {
            if (!a.empty()) return {true, a};
        }
        return {false, ""};
    }
    if (name == "upper")      return {true, args.empty() ? "" : toUpper(args[0])};
    if (name == "lower")      return {true, args.empty() ? "" : toLower(args[0])};
    if (name == "capitalize") return {true, args.empty() ? "" : capitalize(args[0])};

    // Should not reach here — knownWildcards() and this switch must stay in sync.
    return {false, ""};
}

// Splits an argument list `(a, b(c, d), e)` at top-level commas. Returns the
// raw (un-expanded) sub-template strings and the offset *past* the closing
// `)`. Caller is expected to have advanced past the opening `(`.
//
// Returns std::string_view::npos in `endOut` when the input ends before the
// matching close — caller treats this as "not a function wildcard after all"
// and falls back to emitting the name literally.
std::vector<std::string> parseArgList(std::string_view s,
                                      std::size_t start,
                                      std::size_t& endOut) {
    std::vector<std::string> args;
    std::string current;
    int depth = 1; // the opening '(' already consumed by caller
    std::size_t i = start;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '(') {
            ++depth;
            current.push_back(c);
        } else if (c == ')') {
            --depth;
            if (depth == 0) {
                args.push_back(current);
                endOut = i + 1;
                return args;
            }
            current.push_back(c);
        } else if (c == ',' && depth == 1) {
            args.push_back(current);
            current.clear();
            // Trim leading spaces after the comma so `a, b` and `a,b` behave identically.
            while (i + 1 < s.size() && s[i + 1] == ' ') ++i;
        } else {
            current.push_back(c);
        }
    }
    endOut = std::string_view::npos;
    return args;
}

ExpandResult expandImpl(std::string_view tmpl, const MetadataContext& ctx) {
    ExpandResult out;
    out.value.reserve(tmpl.size());
    std::size_t i = 0;
    while (i < tmpl.size()) {
        char c = tmpl[i];
        if (c != '$' || i + 1 >= tmpl.size() || !isIdentChar(tmpl[i + 1])) {
            out.value.push_back(c);
            ++i;
            continue;
        }

        // Scan the identifier run after '$'.
        std::size_t j = i + 1;
        while (j < tmpl.size() && isIdentChar(tmpl[j])) ++j;
        std::string_view fullName = tmpl.substr(i + 1, j - i - 1);

        // Longest-match against known wildcards (handles "ucs_cat" inside
        // "ucs_cat_$x" — see WwiseStructureTemplate.h wildcards section).
        const auto& known = knownWildcards();
        std::string matched;
        std::size_t matchedLen = 0;
        for (std::size_t len = fullName.size(); len > 0; --len) {
            std::string candidate(fullName.substr(0, len));
            if (known.count(candidate)) {
                matched = std::move(candidate);
                matchedLen = len;
                break;
            }
        }
        if (matched.empty()) {
            // Unknown wildcard — emit the "$name" run literally so ordinary
            // text like "price: $5" survives unmolested.
            out.value.push_back('$');
            out.value.append(fullName.data(), fullName.size());
            i = j;
            continue;
        }

        std::size_t nextIdx = i + 1 + matchedLen;

        // If a '(' follows immediately, parse args (recursively expanded).
        std::vector<std::string> args;
        if (nextIdx < tmpl.size() && tmpl[nextIdx] == '(') {
            std::size_t endPos = 0;
            auto rawArgs = parseArgList(tmpl, nextIdx + 1, endPos);
            if (endPos == std::string_view::npos) {
                // Unterminated — emit literal fallback.
                out.value.push_back('$');
                out.value.append(matched);
                i = nextIdx;
                continue;
            }
            for (const auto& raw : rawArgs) {
                ExpandResult sub = expandImpl(raw, ctx);
                args.push_back(std::move(sub.value));
                for (auto& u : sub.unresolvedTokens) {
                    out.unresolvedTokens.push_back(std::move(u));
                }
            }
            nextIdx = endPos;
        }

        Resolved r = resolve(matched, args, ctx);
        if (!r.resolved) {
            out.unresolvedTokens.push_back(matched);
        } else {
            out.value.append(r.value);
        }
        i = nextIdx;
    }
    return out;
}

} // namespace

ExpandResult expandWildcards(const std::string& tmpl, const MetadataContext& ctx) {
    return expandImpl(std::string_view(tmpl), ctx);
}

std::vector<WwiseImportItem> expandStructure(
    const WwiseTransferConfig& config,
    const std::vector<MetadataContext>& contexts,
    const std::vector<std::string>& sourceWavs) {

    const std::size_t n = std::min(contexts.size(), sourceWavs.size());
    std::vector<WwiseImportItem> items;
    items.reserve(n);

    for (std::size_t k = 0; k < n; ++k) {
        // Clone the caller's context so we can inject index/count + userVars
        // without mutating the caller's view.
        MetadataContext ctx = contexts[k];
        ctx.index = static_cast<int>(k + 1);
        ctx.count = static_cast<int>(n);
        // Merge config-level userVars in (per-file userVars already on the
        // incoming context take precedence — lets callers override per file).
        for (const auto& kv : config.userVars) {
            ctx.userVars.emplace(kv.first, kv.second);
        }

        WwiseImportItem item;
        item.sourceWav = sourceWavs[k];

        ExpandResult orig = expandWildcards(config.originalsSubfolder, ctx);
        item.originalsRelPath = std::move(orig.value);
        for (auto& t : orig.unresolvedTokens) item.unresolvedTokens.push_back(std::move(t));

        item.objectPath.reserve(config.structure.size());
        item.propertyTemplatePaths.reserve(config.structure.size());
        item.objectTypes.reserve(config.structure.size());
        for (const auto& layer : config.structure) {
            ExpandResult nameRes = expandWildcards(layer.nameTemplate, ctx);
            item.objectPath.push_back(std::move(nameRes.value));
            for (auto& t : nameRes.unresolvedTokens) item.unresolvedTokens.push_back(std::move(t));
            item.propertyTemplatePaths.push_back(layer.useTemplate ? layer.propertyTemplatePath : std::string{});
            item.objectTypes.push_back(layer.type);
        }
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace wwise
}
