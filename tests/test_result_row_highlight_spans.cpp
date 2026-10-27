#include "ResultRowHighlightSpans.h"

#include <cstdio>
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

int main() {
    {
        std::vector<std::pair<size_t, size_t>> spans;
        AppendResultRowHighlightSpan(spans, 2, 3);
        AppendResultRowHighlightSpan(spans, 0, 1);
        expect(spans.size() == 2 && spans[0] == std::pair<size_t, size_t>(0, 1) && spans[1] == std::pair<size_t, size_t>(2, 3),
               "later-then-earlier hits keep both spans");
    }
    {
        std::vector<std::pair<size_t, size_t>> spans;
        AppendResultRowHighlightSpan(spans, 0, 3);
        AppendResultRowHighlightSpan(spans, 4, 9);
        expect(spans.size() == 2 && spans[0] == std::pair<size_t, size_t>(0, 3) && spans[1] == std::pair<size_t, size_t>(4, 9),
               "sorted non-overlapping hits stay two spans");
    }
    {
        std::vector<std::pair<size_t, size_t>> spans;
        AppendResultRowHighlightSpan(spans, 0, 3);
        AppendResultRowHighlightSpan(spans, 2, 5);
        expect(spans.size() == 1 && spans[0] == std::pair<size_t, size_t>(0, 5),
               "overlapping later hit merges into the earlier span");
    }
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
