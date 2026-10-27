#include "SettingsBodyScrollHeight.h"

#include <cstdio>

static int g_pass = 0;
static int g_fail = 0;

static void expect(bool cond, const char* label) {
    if (cond) {
        std::printf("PASS: %s\n", label);
        ++g_pass;
    } else {
        std::printf("FAIL: %s\n", label);
        ++g_fail;
    }
}

int main() {
    {
        expect(ShouldUpdateSettingsBodyContentHeight(0.0f, 400.0f, false),
               "uninitialized stored height accepts the first measurement");
        expect(ShouldUpdateSettingsBodyContentHeight(-1.0f, 400.0f, true),
               "non-positive stored height accepts measurement even when near bottom");
    }
    {
        expect(ShouldUpdateSettingsBodyContentHeight(400.0f, 480.0f, false),
               "away from bottom, growth of 80px is accepted");
        expect(ShouldUpdateSettingsBodyContentHeight(400.0f, 320.0f, false),
               "away from bottom, shrink of 80px is accepted");
    }
    {
        expect(!ShouldUpdateSettingsBodyContentHeight(400.0f, 401.0f, false),
               "away from bottom, sub-2px jitter is ignored");
        expect(!ShouldUpdateSettingsBodyContentHeight(400.0f, 399.0f, true),
               "near bottom, sub-2px jitter is ignored");
        expect(!ShouldUpdateSettingsBodyContentHeight(400.0f, 401.5f, true),
               "near bottom, 1.5px growth is still jitter and ignored");
    }
    {
        expect(ShouldUpdateSettingsBodyContentHeight(400.0f, 520.0f, true),
               "near bottom, expanding a collapsing header must accept the taller measurement");
        expect(ShouldUpdateSettingsBodyContentHeight(400.0f, 402.0f, true),
               "near bottom, growth of exactly 2px is accepted");
        expect(ShouldUpdateSettingsBodyContentHeight(400.0f, 300.0f, true),
               "near bottom, real shrink of 100px is still accepted");
    }
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
