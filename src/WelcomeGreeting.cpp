// src/WelcomeGreeting.cpp
// Pure implementation of welcome-overlay greeting selection (band only —
// phrase text lives in the localization ledger since 7-A slice-4).
#include "WelcomeGreeting.h"

#include <cctype>

namespace WelcomeGreeting {
namespace {

int NormalizeHour(int hour) {
    int normalized = hour % 24;
    if (normalized < 0) {
        normalized += 24;
    }
    return normalized;
}

bool IsAsciiWhitespace(unsigned char ch) {
    return std::isspace(ch) != 0;
}

std::string TrimAsciiWhitespace(const std::string& value) {
    size_t first = 0;
    while (first < value.size() && IsAsciiWhitespace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    size_t last = value.size();
    while (last > first && IsAsciiWhitespace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    return value.substr(first, last - first);
}

} // namespace

Band BandForHour(int hour) {
    const int normalizedHour = NormalizeHour(hour);
    if (normalizedHour >= 5 && normalizedHour <= 10) {
        return Band::Morning;
    }
    if (normalizedHour >= 11 && normalizedHour <= 13) {
        return Band::Noon;
    }
    if (normalizedHour >= 14 && normalizedHour <= 17) {
        return Band::Afternoon;
    }
    if (normalizedHour >= 18 && normalizedHour <= 22) {
        return Band::Evening;
    }
    return Band::LateNight;
}

Greeting ResolveGreeting(int hour, const std::string& name) {
    Greeting greeting;
    greeting.name = TrimAsciiWhitespace(name);
    greeting.hasName = !greeting.name.empty();
    greeting.band = greeting.hasName ? BandForHour(hour) : Band::WelcomeBack;
    if (!greeting.hasName) {
        greeting.name.clear();
    }
    return greeting;
}

} // namespace WelcomeGreeting
