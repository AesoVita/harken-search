#include "src/AppState.h"

#include <cstdio>

int main() {
    // ResolveReadingFontSize precedence: reading > list > font > default.
    if (ResolveReadingFontSize(true, 20, true, 16, true, 12, 14) != 20) {
        std::printf("FAIL: explicit readingFontSize should win\n");
        return 1;
    }
    if (ResolveReadingFontSize(false, 0, true, 16, true, 12, 14) != 16) {
        std::printf("FAIL: legacy listFontSize should be used when readingFontSize absent\n");
        return 1;
    }
    if (ResolveReadingFontSize(false, 0, false, 0, true, 12, 14) != 12) {
        std::printf("FAIL: legacy fontSize should be used when reading+list absent\n");
        return 1;
    }
    if (ResolveReadingFontSize(false, 0, false, 0, false, 0, 14) != 14) {
        std::printf("FAIL: fresh config should fall back to default\n");
        return 1;
    }

    // ComputeReadingRowHeight: max(densityFloor, font + pad).
    if (ComputeReadingRowHeight(28, 14, 4) != 28) {
        std::printf("FAIL: small font should keep density floor\n");
        return 1;
    }
    if (ComputeReadingRowHeight(28, 24, 4) != 28) {
        // 24+4=28, tie -> floor (not strictly greater)
        std::printf("FAIL: tie should resolve to density floor\n");
        return 1;
    }
    if (ComputeReadingRowHeight(20, 24, 4) != 28) {
        std::printf("FAIL: large font should grow row height above density floor\n");
        return 1;
    }
    if (ComputeReadingRowHeight(36, 24, 4) != 36) {
        std::printf("FAIL: comfortable density floor should hold when font is small\n");
        return 1;
    }

    // getEffectiveRowHeight() must route through ComputeReadingRowHeight.
    {
        AppSettings s;
        s.listDensity = ListDensity::Normal;   // density floor 28
        s.readingFontSize = 14;                 // 14+4=18 < 28 -> floor 28
        if (s.getEffectiveRowHeight() != 28) {
            std::printf("FAIL: normal density with small reading font should be 28\n");
            return 1;
        }
        s.listDensity = ListDensity::Compact;   // density floor 20
        s.readingFontSize = 24;                 // 24+4=28 > 20 -> 28
        if (s.getEffectiveRowHeight() != 28) {
            std::printf("FAIL: compact density with large reading font should grow to 28\n");
            return 1;
        }
    }

    std::printf("PASS: reading font size resolution and row height growth are correct\n");
    return 0;
}
