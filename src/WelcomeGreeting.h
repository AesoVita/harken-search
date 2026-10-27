// src/WelcomeGreeting.h
// Pure, WebView2-free greeting selection for the welcome overlay.
// Time-of-day and name selection is a pure value operation. The caller maps
// the returned Band to a localization ledger key for all supported languages.
#pragma once

#include <string>

// @stage7-contract
// API contract: greeting helpers are pure synchronous selection functions.
// They borrow the input name only for the call and return owned band/name
// values, with whitespace/invalid-hour handling preserved by existing
// fallback semantics and no WebView/global state dependency. Phrase TEXT
// lives in the localization ledger, not here.
namespace WelcomeGreeting {

enum class Band {
    Morning,     // 05-10
    Noon,        // 11-13
    Afternoon,   // 14-17
    Evening,     // 18-22
    LateNight,   // 23-04
    WelcomeBack, // empty/whitespace name fallback
};

struct Greeting {
    Band band;
    std::string name;    // trimmed user name, empty when none
    bool hasName = false;
};

// Pure band selection by local hour [0,23] (spec §5 bands).
Band BandForHour(int hour);

// Full greeting: empty/whitespace name -> Band::WelcomeBack with hasName=false.
Greeting ResolveGreeting(int hour, const std::string& name);

} // namespace WelcomeGreeting
