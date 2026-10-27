#include "SearchIntent.h"

#include <cstdio>
#include <string>

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

static void checkEq(const std::string& actual, const std::string& expected, const char* desc)
{
    if (actual == expected) {
        std::printf("PASS: %s\n", desc);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n  expected: %s\n    actual: %s\n", desc, expected.c_str(), actual.c_str());
        ++g_fail;
    }
}

static void test_parse_all_mode()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/all name contains boom category = weapon");
    check(r.ok, "ParseSlashSearchIntent accepts /all with two clauses");
    checkEq(r.keyword, "name:\"boom\" category:\"weapon\"", "/all compiles clauses as AND terms");
}

static void test_parse_any_mode()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/any name contains boom category = weapon");
    check(r.ok, "ParseSlashSearchIntent accepts /any with two clauses");
    checkEq(r.keyword, "(name:\"boom\" OR category:\"weapon\")", "/any compiles clauses as OR group");

    auto freeTerms = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/any boom metal");
    check(freeTerms.ok, "ParseSlashSearchIntent accepts /any free terms");
    checkEq(freeTerms.keyword, "(boom OR metal)", "/any compiles free terms as OR terms");
}

static void test_parse_where_equals()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/where category = weapon");
    check(r.ok, "ParseSlashSearchIntent accepts /where field equals value");
    checkEq(r.keyword, "category:\"weapon\"", "/where equals compiles to field phrase");
}

static void test_parse_where_contains_quoted()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/where desc contains \"metal impact\"");
    check(r.ok, "ParseSlashSearchIntent accepts /where field contains quoted value");
    checkEq(r.keyword, "desc:\"metal impact\"", "/where contains preserves quoted value");
}

static void test_parse_negated_clause()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/not category = weapon");
    check(r.ok, "ParseSlashSearchIntent accepts /not field equals value");
    checkEq(r.keyword, "-category:\"weapon\"", "/not compiles to negative field term");
}

static void test_reject_invalid_field()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/where rating = five");
    check(!r.ok, "ParseSlashSearchIntent rejects invalid field");
    check(!r.error.empty(), "Invalid field returns user-facing error");

    auto multi = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/all rating = five category = weapon");
    check(!multi.ok, "ParseSlashSearchIntent rejects invalid field in /all clause list");
}

static void test_reject_invalid_operator()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/where category starts weapon");
    check(!r.ok, "ParseSlashSearchIntent rejects invalid operator");
    check(!r.error.empty(), "Invalid operator returns user-facing error");
}

static void test_reject_unterminated_quote()
{
    auto r = HarkenSearch::SearchIntent::CompileSlashSearchInputToKeyword("/where name = \"broken");
    check(!r.ok, "CompileSlashSearchInputToKeyword rejects unterminated quoted slash value");
    check(!r.error.empty(), "Unterminated quote returns user-facing error");
}

static void test_reject_unrepresentable_any_side_filter()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/any path = Boom category = weapon");
    check(!r.ok, "ParseSlashSearchIntent rejects /any path side-filter semantics in v1");
    check(!r.error.empty(), "Unrepresentable /any side-filter returns user-facing error");
}

static void test_compile_quoted_values()
{
    auto r = HarkenSearch::SearchIntent::ParseSlashSearchIntent("/where name = \"UI \"\"Clicks\"\"\"");
    check(r.ok, "ParseSlashSearchIntent accepts doubled quotes inside phrase");
    checkEq(r.keyword, "name:\"UI \"\"Clicks\"\"\"", "Compile doubles embedded quotes for SearchQuery phrase syntax");
}

static void test_non_slash_pass_through()
{
    auto r = HarkenSearch::SearchIntent::CompileSlashSearchInputToKeyword("boom category:weapon");
    check(r.ok, "CompileSlashSearchInputToKeyword accepts non-slash search");
    checkEq(r.keyword, "boom category:weapon", "Non-slash search passes through unchanged");
}

int main()
{
    test_parse_all_mode();
    test_parse_any_mode();
    test_parse_where_equals();
    test_parse_where_contains_quoted();
    test_parse_negated_clause();
    test_reject_invalid_field();
    test_reject_invalid_operator();
    test_reject_unterminated_quote();
    test_reject_unrepresentable_any_side_filter();
    test_compile_quoted_values();
    test_non_slash_pass_through();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
