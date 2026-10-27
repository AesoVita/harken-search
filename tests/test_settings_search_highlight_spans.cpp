#include "SettingsSearchHighlightSpans.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

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

static void expectNear(float actual, float expected, float tolerance, const char* label) {
    const bool withinTolerance = std::fabs(actual - expected) <= tolerance;
    if (withinTolerance) {
        std::printf("PASS: %s (got %.6f)\n", label, actual);
        ++g_pass;
    } else {
        std::printf("FAIL: %s (got %.6f, expected %.6f +/- %.4f)\n", label, actual, expected, tolerance);
        ++g_fail;
    }
}

int main() {
    const std::string guidedZh = u8"引导模式";
    const std::string guidedPrefix = u8"引导";
    const std::string guidedEn = "Guided Mode";
    const std::string keywords =
        u8"引导模式 Guided Mode 界面 引导 tooltip ui guided gui";

    {
        const auto spans = BuildSettingsLabelHighlightSpans(guidedZh, "gui", keywords);
        expect(!spans.empty(), "gui against chinese Guided Mode yields a visible span");
        expect(spans.size() == 1 && spans[0].first == 0 && spans[0].second == guidedPrefix.size(),
               "gui highlights the 引导 affix, not the whole 引导模式 row");
    }
    {
        const auto spans = BuildSettingsLabelHighlightSpans(guidedEn, "gui", keywords);
        expect(spans.size() == 1 && spans[0].first == 0 && spans[0].second == std::string("Guided").size(),
               "gui highlights Guided on the english label");
    }
    {
        const auto spans = BuildSettingsLabelHighlightSpans(guidedZh, "guided");
        expect(spans.empty(), "english keyword that is not in the chinese label yields no literal span");
    }
    {
        std::vector<std::pair<size_t, size_t>> spans =
            BuildSettingsLabelHighlightSpans(guidedZh, "guided", keywords);
        expect(!spans.empty() && spans[0].first == 0 && spans[0].second == guidedPrefix.size(),
               "keyword-index hit maps guided back onto 引导");
        EnsureActiveSettingsLabelHighlight(spans, guidedZh, "guided", guidedZh);
        expect(spans.size() == 1 && spans[0].second == guidedPrefix.size(),
               "active fallback does not widen an already mapped affix to the whole label");
    }
    {
        const auto spans = BuildSettingsLabelHighlightSpans(u8"输出硬件", u8"输出");
        expect(spans.size() == 1 && spans[0].first == 0 && spans[0].second == std::string(u8"输出").size(),
               "chinese query highlights the matching prefix of the drawn header");
    }
    {
        std::vector<std::pair<size_t, size_t>> spans =
            BuildSettingsLabelHighlightSpans(u8"系统性能模式", "fps");
        EnsureActiveSettingsLabelHighlight(spans, u8"系统性能模式", "fps", u8"节能模式");
        expect(spans.empty(), "fallback highlight only applies to the active label");
    }
    {
        const std::string appearance = u8"外观";
        const auto spans = BuildSettingsLabelHighlightSpans(appearance, appearance);
        expect(spans.size() == 1 && spans[0].first == 0 && spans[0].second == appearance.size(),
               "chinese query 外观 vs label 外观 yields a span");
    }
    {
        const std::string behavior = u8"搜索行为";
        const std::string query = u8"搜索";
        const auto spans = BuildSettingsLabelHighlightSpans(behavior, query);
        expect(spans.size() == 1 && spans[0].first == 0 && spans[0].second == query.size(),
               "搜索 vs 搜索行为 only lights 搜索");
    }
    {
        expect(SettingsLabelIsCurrentHit(u8"外观", u8"外观"),
               "exact current-hit label matches");
        expect(SettingsLabelIsCurrentHit(u8"v  外观", u8"外观"),
               "chevron-prefixed section title still matches active 外观");
        expect(!SettingsLabelIsCurrentHit(u8"实时搜索", u8"搜索行为"),
               "other controls on the page are not the current 1/N hit");
    }
    {
        const std::string drawn = u8"v  外观";
        const std::string title = u8"外观";
        std::vector<std::pair<size_t, size_t>> spans;
        EnsureActiveSettingsLabelHighlight(spans, drawn, title, title);
        const size_t titleStart = drawn.find(title);
        expect(titleStart != std::string::npos &&
                   spans.size() == 1 &&
                   spans[0].first == titleStart &&
                   spans[0].second == titleStart + title.size(),
               "fallback lights the title glyphs after the chevron prefix");
    }
    {
        constexpr float tolerance = 1e-4f;
        expectNear(SettingsSearchHighlightAlpha(0.0f), 0.55f, tolerance,
                   "elapsed=0 starts at pulse high 0.55");
        expectNear(SettingsSearchHighlightAlpha(0.2f), 0.15f, tolerance,
                   "elapsed=0.2 (half cycle) is pulse low 0.15");
        expectNear(SettingsSearchHighlightAlpha(0.4f), 0.55f, tolerance,
                   "elapsed=0.4 (one cycle) returns to pulse high 0.55");
        expect(std::fabs(SettingsSearchHighlightAlpha(0.999f) - 0.38f) > tolerance,
               "elapsed=0.999 is still pulsing, not steady 0.38");
        expectNear(SettingsSearchHighlightAlpha(1.0f), 0.38f, tolerance,
                   "elapsed=1.0 is steady 0.38");
        expectNear(SettingsSearchHighlightAlpha(5.0f), 0.38f, tolerance,
                   "elapsed=5.0 stays steady 0.38");
        expectNear(SettingsSearchHighlightAlpha(-0.1f), 0.38f, tolerance,
                   "negative elapsed is steady 0.38");
    }
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
