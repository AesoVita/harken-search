#pragma once

#include <cmath>

// Settings-body content height: accept any |measured-stored| >= 2px,
// including growth while scrolled to the bottom (expanding a collapsing
// header). Sub-2px jitter is ignored. nearScrollBottom is kept so callers
// can pass the existing freeze probe; it must not block growth.
inline bool ShouldUpdateSettingsBodyContentHeight(
    float storedContentH,
    float measuredContentH,
    bool nearScrollBottom) {
    (void)nearScrollBottom;
    if (storedContentH <= 0.0f) {
        return true;
    }
    return std::fabs(measuredContentH - storedContentH) >= 2.0f;
}
