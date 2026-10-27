// test_sidebar_order.cpp
// Unit test for the sidebar section order parser and drag-reorder logic used by
// the VS Code-style Activity Bar (main.cpp RenderSidebarPanelContent).
//
// The parser and reorder algorithms are inline in main.cpp; this test replicates
// the exact logic verbatim to verify correctness without pulling in main.cpp.

#include <cstdio>
#include <cstdlib>
#include <string>

static constexpr int kCount = 6;
static const int kDefaults[kCount] = {0, 5, 1, 4, 2, 3};

// Mirror of the parse-and-validate block in main.cpp LoadSettings.
// Returns true if input was a valid permutation and outArr was populated;
// otherwise outArr is left unchanged (caller seeded with defaults).
static bool ParseSidebarSectionOrder(const std::string& s, int* outArr) {
    int parsed[kCount] = {};
    int n = 0;
    size_t p = 0;
    bool parseOk = true;
    while (p < s.size() && n < kCount) {
        size_t c = s.find(',', p);
        std::string tok = s.substr(p, c == std::string::npos ? std::string::npos : c - p);
        try { parsed[n++] = std::stoi(tok); }
        catch (...) { parseOk = false; break; }
        if (c == std::string::npos) break;
        p = c + 1;
    }
    bool ok = parseOk && (n == kCount);
    if (ok) {
        bool seen[kCount] = {};
        for (int i = 0; i < kCount; i++) {
            if (parsed[i] < 0 || parsed[i] >= kCount || seen[parsed[i]]) { ok = false; break; }
            seen[parsed[i]] = true;
        }
    }
    if (!ok) return false;
    for (int i = 0; i < kCount; i++) outArr[i] = parsed[i];
    return true;
}

// Mirror of the drag-reorder block inside DrawSidebarActivityBar.
// Moves `from` sectionId to the slot currently holding `to`, shifting intermediate
// entries in the opposite direction. Returns true if mutation happened.
static bool DragReorder(int* ord, int from, int to) {
    if (from == to) return false;
    int iFrom = -1, iTo = -1;
    for (int i = 0; i < kCount; i++) { if (ord[i] == from) iFrom = i; if (ord[i] == to) iTo = i; }
    if (iFrom < 0 || iTo < 0) return false;
    int v = ord[iFrom];
    if (iFrom < iTo) { for (int i = iFrom; i < iTo; i++) ord[i] = ord[i+1]; ord[iTo] = v; }
    else             { for (int i = iFrom; i > iTo; i--) ord[i] = ord[i-1]; ord[iTo] = v; }
    return true;
}

// Serialize order back to CSV (matches SaveSettings).
static std::string SerializeOrder(const int* ord) {
    std::string out;
    for (int i = 0; i < kCount; i++) {
        if (i) out += ",";
        out += std::to_string(ord[i]);
    }
    return out;
}

static int g_fails = 0;

#define EXPECT_EQ_ARR(got, want, desc) do {                                   \
    bool _match = true;                                                        \
    for (int _i = 0; _i < kCount; ++_i) if (got[_i] != want[_i]) _match = false; \
    if (_match) { printf("PASS: %s\n", desc); }                                \
    else {                                                                     \
        printf("FAIL: %s — got {", desc);                                      \
        for (int _i = 0; _i < kCount; ++_i) printf("%s%d", _i?",":"", got[_i]);\
        printf("}, want {");                                                    \
        for (int _i = 0; _i < kCount; ++_i) printf("%s%d", _i?",":"", want[_i]);\
        printf("}\n");                                                          \
        g_fails++;                                                              \
    }                                                                          \
} while(0)

#define EXPECT(cond, desc) do {                                               \
    if (cond) { printf("PASS: %s\n", desc); }                                 \
    else      { printf("FAIL: %s\n", desc); g_fails++; }                      \
} while(0)

int main() {
    // 1. Valid permutation parses successfully.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("2,0,1,3,4,5", out);
        EXPECT(ok, "valid CSV parses successfully");
        const int want[kCount] = {2,0,1,3,4,5};
        EXPECT_EQ_ARR(out, want, "valid CSV yields expected permutation");
    }

    // 2. Duplicates rejected — defaults preserved.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("0,0,1,2,3,4", out);
        EXPECT(!ok, "duplicates rejected");
        EXPECT_EQ_ARR(out, kDefaults, "duplicates leave defaults intact");
    }

    // 3. Out-of-range rejected.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("0,1,2,3,4,99", out);
        EXPECT(!ok, "out-of-range index rejected");
        EXPECT_EQ_ARR(out, kDefaults, "out-of-range leaves defaults intact");
    }

    // 4. Too few tokens rejected.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("0,1,2", out);
        EXPECT(!ok, "too-few-tokens rejected");
        EXPECT_EQ_ARR(out, kDefaults, "too few tokens leaves defaults intact");
    }

    // 5. Non-numeric rejected.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("a,b,c,d,e,f", out);
        EXPECT(!ok, "non-numeric CSV rejected");
        EXPECT_EQ_ARR(out, kDefaults, "non-numeric leaves defaults intact");
    }

    // 6. Negative index rejected.
    {
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder("0,1,2,3,4,-1", out);
        EXPECT(!ok, "negative index rejected");
        EXPECT_EQ_ARR(out, kDefaults, "negative index leaves defaults intact");
    }

    // 7. Round-trip: serialize then parse yields identical array.
    {
        int in[kCount]  = {3,1,4,0,5,2};
        std::string csv = SerializeOrder(in);
        int out[kCount] = {0,5,1,4,2,3};
        bool ok = ParseSidebarSectionOrder(csv, out);
        EXPECT(ok, "round-trip CSV parses");
        EXPECT_EQ_ARR(out, in, "round-trip preserves array");
    }

    // 8. Drag reorder: move sectionId 0 from slot 0 onto slot holding id 4
    //    (which is at position 3 in {0,5,1,4,2,3}). Expected: {5,1,4,0,2,3}.
    {
        int ord[kCount] = {0,5,1,4,2,3};
        bool changed = DragReorder(ord, /*from*/0, /*to*/4);
        EXPECT(changed, "drag forward reports mutation");
        const int want[kCount] = {5,1,4,0,2,3};
        EXPECT_EQ_ARR(ord, want, "drag forward places sectionId 0 after id 4");
    }

    // 9. Drag reorder reverse: move sectionId 3 (at slot 5) onto slot holding
    //    sectionId 0 (at slot 0). Expected: {3,0,5,1,4,2}.
    {
        int ord[kCount] = {0,5,1,4,2,3};
        bool changed = DragReorder(ord, /*from*/3, /*to*/0);
        EXPECT(changed, "drag backward reports mutation");
        const int want[kCount] = {3,0,5,1,4,2};
        EXPECT_EQ_ARR(ord, want, "drag backward places sectionId 3 before id 0");
    }

    // 10. Drag onto self is a no-op.
    {
        int ord[kCount] = {0,5,1,4,2,3};
        bool changed = DragReorder(ord, /*from*/4, /*to*/4);
        EXPECT(!changed, "drag onto self reports no change");
        const int want[kCount] = {0,5,1,4,2,3};
        EXPECT_EQ_ARR(ord, want, "drag onto self leaves array intact");
    }

    if (g_fails == 0) { printf("\nAll sidebar order tests PASSED.\n"); return 0; }
    printf("\n%d sidebar order test(s) FAILED.\n", g_fails);
    return 1;
}
