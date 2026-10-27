// tests/test_search_commands.cpp
// Unit tests for SearchCommands module (Task 4 — slash-advanced-search).
// Self-contained: no test framework. Prints PASS/FAIL, exits 0/1.

#include "SearchCommands.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// ── helpers ──────────────────────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* desc)
{
    if (ok) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", desc);
        ++g_fail;
    }
}

// Check that a vector of const SearchCommand* contains exactly one element
// whose name equals `name`.
static bool matchesSingle(const std::vector<const SearchCommands::SearchCommand*>& v,
                           std::string_view name)
{
    if (v.size() != 1) return false;
    return v[0]->name == name;
}

// ── GetAllCommands ────────────────────────────────────────────────────────────

static void test_GetAllCommands()
{
    const auto& cmds = SearchCommands::GetAllCommands();

    check(cmds.size() == 9, "GetAllCommands returns 9 commands");

    // Expected names (order-agnostic)
    const std::string_view expected[] = {"all", "any", "where", "or", "not", "exact", "group", "category", "path"};
    for (std::string_view name : expected) {
        bool found = false;
        for (const auto& cmd : cmds) {
            if (cmd.name == name) { found = true; break; }
        }
        char buf[128];
        std::snprintf(buf, sizeof(buf), "GetAllCommands contains \"%.*s\"",
                      (int)name.size(), name.data());
        check(found, buf);
    }
}

// ── MatchCommandsByPrefix ─────────────────────────────────────────────────────

static void test_MatchCommandsByPrefix()
{
    namespace SC = SearchCommands;

    // Empty prefix → all 9
    {
        auto res = SC::MatchCommandsByPrefix("");
        check(res.size() == 9, "MatchCommandsByPrefix(\"\") returns 9 commands");
    }

    // "ca" → ["category"]
    {
        auto res = SC::MatchCommandsByPrefix("ca");
        check(matchesSingle(res, "category"), "MatchCommandsByPrefix(\"ca\") returns [category]");
    }

    // "or" → ["or"] (exact match, not prefix-of-longer)
    {
        auto res = SC::MatchCommandsByPrefix("or");
        check(matchesSingle(res, "or"), "MatchCommandsByPrefix(\"or\") returns [or]");
    }

    // "n" → ["not"]
    {
        auto res = SC::MatchCommandsByPrefix("n");
        check(matchesSingle(res, "not"), "MatchCommandsByPrefix(\"n\") returns [not]");
    }

    // "al" → ["all"]
    {
        auto res = SC::MatchCommandsByPrefix("al");
        check(matchesSingle(res, "all"), "MatchCommandsByPrefix(\"al\") returns [all]");
    }

    // "an" → ["any"]
    {
        auto res = SC::MatchCommandsByPrefix("an");
        check(matchesSingle(res, "any"), "MatchCommandsByPrefix(\"an\") returns [any]");
    }

    // "w" → ["where"]
    {
        auto res = SC::MatchCommandsByPrefix("w");
        check(matchesSingle(res, "where"), "MatchCommandsByPrefix(\"w\") returns [where]");
    }

    // "xyz" → empty
    {
        auto res = SC::MatchCommandsByPrefix("xyz");
        check(res.empty(), "MatchCommandsByPrefix(\"xyz\") returns empty");
    }

    // "p" → ["path"]
    {
        auto res = SC::MatchCommandsByPrefix("p");
        check(matchesSingle(res, "path"), "MatchCommandsByPrefix(\"p\") returns [path]");
    }

    // multi-character prefix that narrows to one result
    {
        auto res = SC::MatchCommandsByPrefix("gro");
        check(matchesSingle(res, "group"), "MatchCommandsByPrefix(\"gro\") returns [group]");
    }

    // Verify returned pointer is within the commands vector's range
    {
        const auto& all = SC::GetAllCommands(); // used for pointer-range check below
        auto res = SC::MatchCommandsByPrefix("ca");
        bool inRange = false;
        if (!res.empty()) {
            const SC::SearchCommand* begin = all.data();
            const SC::SearchCommand* end   = all.data() + all.size();
            inRange = (res[0] >= begin && res[0] < end);
        }
        check(inRange, "MatchCommandsByPrefix returns pointer into GetAllCommands() range");
    }
}

// ── IsTokenBoundary ───────────────────────────────────────────────────────────

static void test_IsTokenBoundary()
{
    namespace SC = SearchCommands;

    // "/" at pos 0 → true (start of buffer)
    check(SC::IsTokenBoundary("/", 0),
          "IsTokenBoundary(\"/\", 0) == true (start of buffer)");

    // "a /" at pos 2 → true (space before)
    check(SC::IsTokenBoundary("a /", 2),
          "IsTokenBoundary(\"a /\", 2) == true (space before)");

    // "a\t/" at pos 2 → true (tab before)
    check(SC::IsTokenBoundary("a\t/", 2),
          "IsTokenBoundary(\"a\\t/\", 2) == true (tab before)");

    // "a/" at pos 1 → false (non-whitespace before)
    check(!SC::IsTokenBoundary("a/", 1),
          "IsTokenBoundary(\"a/\", 1) == false (non-whitespace before)");

    // "path:C:/foo" at pos 6 → false (colon before)
    check(!SC::IsTokenBoundary("path:C:/foo", 6),
          "IsTokenBoundary(\"path:C:/foo\", 6) == false (colon before)");

    // "\"foot step\" /" at pos 12 → true (after closed quote + space)
    {
        std::string buf = "\"foot step\" /";
        // buf[0]='"', buf[10]='"', buf[11]=' ', buf[12]='/'; slashPos=12
        check(SC::IsTokenBoundary(buf, 12),
              "IsTokenBoundary(\"\\\"foot step\\\" /\", 12) == true (after closed quote)");
    }

    // "\"foot /step\"" at pos 6 → false (inside unclosed quote)
    {
        std::string buf = "\"foot /step\"";
        // buf[0]='"' opens quote, buf[6]='/', buf[11]='"' closes — but at pos 6
        // we've only seen one unescaped quote before pos 6, so inside string.
        check(!SC::IsTokenBoundary(buf, 6),
              "IsTokenBoundary(\"\\\"foot /step\\\"\", 6) == false (inside quoted string)");
    }

    // Backslash-escaped quotes: \\\"foot step\\\" /  at pos 14 → true
    // The buffer contains:  \"foot step\" /
    // backslash+quote pairs — unescaped quote count = 0 → not inside a string.
    {
        // Construct with actual backslash chars: \, ", f, o, o, t, ' ', s, t, e, p, \, ", ' ', /
        // That is 15 chars. slashPos = 14.
        std::string buf;
        buf += '\\'; buf += '"';       // \" (0,1)
        buf += "foot step";            // (2..10)
        buf += '\\'; buf += '"';       // \" (11,12)
        buf += ' ';                    // (13)
        buf += '/';                    // (14)
        check(SC::IsTokenBoundary(buf, 14),
              "IsTokenBoundary(backslash-quoted /\", 14) == true (escaped quotes don't count)");
    }

    // newline before slash is NOT treated as whitespace (intentional — avoids UTF-8 UB)
    check(!SC::IsTokenBoundary("a\n/", 2), "IsTokenBoundary(\"a\\n/\", 2) -> false (newline not whitespace)");

    // Empty buffer at pos 0 → false (no '/' at that position)
    check(!SC::IsTokenBoundary("", 0),
          "IsTokenBoundary(\"\", 0) == false (empty buffer)");

    // "/" at pos 1 → false (pos >= buffer.size())
    check(!SC::IsTokenBoundary("/", 1),
          "IsTokenBoundary(\"/\", 1) == false (pos out of range)");
}

static void test_QuoteFieldValueForInsert()
{
    namespace SC = SearchCommands;

    check(SC::QuoteFieldValueForInsert("AMB") == "\"AMB\" ",
          "QuoteFieldValueForInsert quotes simple values");

    check(SC::QuoteFieldValueForInsert("E:\\Sound Libraries\\Boom FX") ==
              "\"E:\\Sound Libraries\\Boom FX\" ",
          "QuoteFieldValueForInsert preserves path spaces and drive colon");

    check(SC::QuoteFieldValueForInsert("UI \"Clicks\"") == "\"UI \"\"Clicks\"\"\" ",
          "QuoteFieldValueForInsert doubles embedded quotes");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main()
{
    test_GetAllCommands();
    test_MatchCommandsByPrefix();
    test_IsTokenBoundary();
    test_QuoteFieldValueForInsert();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
