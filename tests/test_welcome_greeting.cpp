// tests/test_welcome_greeting.cpp
// Unit tests for WelcomeGreeting module (welcome overlay).
// 7-A slice-4: the module is band-pure (phrase text moved to the localization
// ledger; the caller maps Band -> LStr), so this suite checks BAND selection,
// hour normalization and name trim/fallback semantics only.
// Self-contained: no test framework. Prints PASS/FAIL, exits 0/1.

#include "WelcomeGreeting.h"

#include <cstdio>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool ok, const char* desc)
{
    if (ok) { std::printf("PASS: %s\n", desc); ++g_pass; }
    else    { std::printf("FAIL: %s\n", desc); ++g_fail; }
}

namespace WG = WelcomeGreeting;

// ── BandForHour: 5 bands incl. edges + normalization ─────────────────────────

static void test_BandForHour()
{
    check(WG::BandForHour(5) == WG::Band::Morning, "05:00 -> Morning");
    check(WG::BandForHour(10) == WG::Band::Morning, "10 band edge -> Morning");
    check(WG::BandForHour(11) == WG::Band::Noon, "11:00 -> Noon");
    check(WG::BandForHour(13) == WG::Band::Noon, "13 band edge -> Noon");
    check(WG::BandForHour(14) == WG::Band::Afternoon, "14:00 -> Afternoon");
    check(WG::BandForHour(17) == WG::Band::Afternoon, "17 band edge -> Afternoon");
    check(WG::BandForHour(18) == WG::Band::Evening, "18:00 -> Evening");
    check(WG::BandForHour(22) == WG::Band::Evening, "22 band edge -> Evening");
    check(WG::BandForHour(23) == WG::Band::LateNight, "23:00 -> LateNight");
    check(WG::BandForHour(0) == WG::Band::LateNight, "00:00 -> LateNight");
    check(WG::BandForHour(4) == WG::Band::LateNight, "04 band edge -> LateNight");
    check(WG::BandForHour(-2) == WG::Band::Evening, "normalize(-2 -> 22) -> Evening");
    check(WG::BandForHour(26) == WG::Band::LateNight, "normalize(26 -> 2) -> LateNight");
}

// ── ResolveGreeting: band + name trim/fallback semantics ─────────────────────

static void test_ResolveGreeting()
{
    {
        WG::Greeting g = WG::ResolveGreeting(7, "Mix");
        check(g.band == WG::Band::Morning, "ResolveGreeting(07, Mix) band == Morning");
        check(g.hasName, "ResolveGreeting(07, Mix) hasName == true");
        check(g.name == "Mix", "ResolveGreeting(07, Mix) name == Mix");
    }
    {
        WG::Greeting g = WG::ResolveGreeting(7, "");
        check(g.band == WG::Band::WelcomeBack, "empty name -> WelcomeBack band");
        check(!g.hasName, "empty name -> hasName == false");
        check(g.name.empty(), "empty name -> name empty");
    }
    {
        WG::Greeting g = WG::ResolveGreeting(20, "   ");
        check(g.band == WG::Band::WelcomeBack, "whitespace name -> WelcomeBack band");
        check(!g.hasName, "whitespace name -> hasName == false");
    }
    {
        WG::Greeting g = WG::ResolveGreeting(15, "Sam");
        check(g.band == WG::Band::Afternoon, "ResolveGreeting(15, Sam) band == Afternoon");
        check(g.hasName && g.name == "Sam", "ResolveGreeting(15, Sam) hasName + name == Sam");
    }
}

int main()
{
    test_BandForHour();
    test_ResolveGreeting();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
