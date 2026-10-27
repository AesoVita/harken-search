// test_utils.cpp — Unit tests for Utils.h/cpp standalone functions
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "../src/Utils.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static bool near(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    std::printf("=== Utils Tests ===\n");

    // ── TrimString ──
    std::printf("\n--- TrimString ---\n");
    {
        CHECK(TrimString("  hello  ") == "hello", "trim spaces");
        CHECK(TrimString("\t\thello\t\t") == "hello", "trim tabs");
        CHECK(TrimString("\r\nhello\r\n") == "hello", "trim CR/LF");
        CHECK(TrimString("  \t \r\n ") == "", "all whitespace -> empty");
        CHECK(TrimString("") == "", "empty -> empty");
        CHECK(TrimString("hello") == "hello", "no whitespace -> unchanged");
        CHECK(TrimString("  hello world  ") == "hello world", "interior space preserved");
    }

    // ── TrimCopy ──
    std::printf("\n--- TrimCopy ---\n");
    {
        CHECK(TrimCopy("  abc  ") == "abc", "TrimCopy trims spaces");
        CHECK(TrimCopy("") == "", "TrimCopy empty");
    }

    // ── SanitizeWindowsName ──
    std::printf("\n--- SanitizeWindowsName ---\n");
    {
        CHECK(SanitizeWindowsName("normal_name") == "normal_name", "clean name unchanged");
        CHECK(SanitizeWindowsName("file<>name") == "file__name", "angle brackets replaced");
        CHECK(SanitizeWindowsName("a:b/c\\d") == "a_b_c_d", "colon/slash/backslash replaced");
        CHECK(SanitizeWindowsName("file\"name") == "file_name", "quote replaced");
        CHECK(SanitizeWindowsName("name|pipe?star*") == "name_pipe_star_", "pipe/question/star replaced");
        CHECK(SanitizeWindowsName("name...") == "name", "trailing dots removed");
        CHECK(SanitizeWindowsName("name   ") == "name", "trailing spaces removed");
        CHECK(SanitizeWindowsName("  name  ") == "name", "leading+trailing spaces trimmed");
        CHECK(SanitizeWindowsName("") == "_", "empty -> underscore");
        CHECK(SanitizeWindowsName("...") == "_", "all dots -> underscore");
        CHECK(SanitizeWindowsName("   ") == "_", "all spaces -> underscore");
    }

    // ── ParseBool ──
    std::printf("\n--- ParseBool ---\n");
    {
        CHECK(ParseBool("true", false) == true, "\"true\" -> true");
        CHECK(ParseBool("TRUE", false) == true, "\"TRUE\" -> true (case insensitive)");
        CHECK(ParseBool("True", false) == true, "\"True\" -> true");
        CHECK(ParseBool("1", false) == true, "\"1\" -> true");
        CHECK(ParseBool("yes", false) == true, "\"yes\" -> true");
        CHECK(ParseBool("on", false) == true, "\"on\" -> true");
        CHECK(ParseBool("false", true) == false, "\"false\" -> false");
        CHECK(ParseBool("FALSE", true) == false, "\"FALSE\" -> false");
        CHECK(ParseBool("0", true) == false, "\"0\" -> false");
        CHECK(ParseBool("no", true) == false, "\"no\" -> false");
        CHECK(ParseBool("off", true) == false, "\"off\" -> false");
        CHECK(ParseBool("maybe", true) == true, "unknown -> default(true)");
        CHECK(ParseBool("maybe", false) == false, "unknown -> default(false)");
        CHECK(ParseBool("", true) == true, "empty -> default(true)");
    }

    // ── ParseInt ──
    std::printf("\n--- ParseInt ---\n");
    {
        CHECK(ParseInt("42", 0) == 42, "\"42\" -> 42");
        CHECK(ParseInt("-10", 0) == -10, "\"-10\" -> -10");
        CHECK(ParseInt("0", 99) == 0, "\"0\" -> 0");
        CHECK(ParseInt("", 99) == 99, "empty -> default");
        CHECK(ParseInt("abc", 99) == 99, "non-numeric -> default");
    }

    // ── ParseFloat ──
    std::printf("\n--- ParseFloat ---\n");
    {
        CHECK(near(ParseFloat("3.14", 0.0f), 3.14f, 0.01f), "\"3.14\" -> 3.14");
        CHECK(near(ParseFloat("-1.5", 0.0f), -1.5f, 0.01f), "\"-1.5\" -> -1.5");
        CHECK(near(ParseFloat("0", 99.0f), 0.0f, 0.01f), "\"0\" -> 0");
        CHECK(near(ParseFloat("", 99.0f), 99.0f, 0.01f), "empty -> default");
        CHECK(near(ParseFloat("abc", 99.0f), 99.0f, 0.01f), "non-numeric -> default");
    }

    // ── NormalizeFolderPathFromKeyForLookup ──
    std::printf("\n--- NormalizeFolderPathFromKeyForLookup ---\n");
    {
        CHECK(NormalizeFolderPathFromKeyForLookup("F|E:\\Sounds") == "E:\\Sounds",
              "valid F| key -> path");
        CHECK(NormalizeFolderPathFromKeyForLookup("F|C:\\Lib\\FX") == "C:\\Lib\\FX",
              "another valid key");
        CHECK(NormalizeFolderPathFromKeyForLookup("F|\\\\?\\E:\\Sounds") == "E:\\Sounds",
              "\\\\?\\ prefix stripped");
        CHECK(NormalizeFolderPathFromKeyForLookup("F|\\?\\E:\\Sounds") == "E:\\Sounds",
              "\\?\\ prefix stripped");
        CHECK(NormalizeFolderPathFromKeyForLookup("") == "",
              "empty -> empty");
        CHECK(NormalizeFolderPathFromKeyForLookup("X|path") == "",
              "wrong prefix -> empty");
        CHECK(NormalizeFolderPathFromKeyForLookup("F|") == "",
              "F| with no path -> empty (too short for drive letter)");
        CHECK(NormalizeFolderPathFromKeyForLookup("F|H|something") == "",
              "F|H| (hash) -> empty");
    }

    // ── ShouldHighlightFolderNodeForLocate ──
    std::printf("\n--- ShouldHighlightFolderNodeForLocate ---\n");
    {
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Sounds", "E:\\Sounds", 1000, 500) == true,
              "matching path within time window");
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Sounds", "E:\\Sounds", 1000, 1500) == false,
              "matching path but past deadline");
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Other", "E:\\Sounds", 1000, 500) == false,
              "non-matching path");
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Sounds", "", 1000, 500) == false,
              "empty highlight path");
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Sounds", "E:\\Sounds", 0, 0) == false,
              "zero deadline");
        CHECK(ShouldHighlightFolderNodeForLocate("E:\\Sounds", "E:\\Sounds", -1, 0) == false,
              "negative deadline");
    }

    // ── ApplyExportDeclickFade ──
    std::printf("\n--- ApplyExportDeclickFade ---\n");
    {
        // Test with 100 frames mono, fade length 10
        const size_t N = 100;
        const size_t fadeLenFrames = 10;
        float samples[N];
        for (size_t i = 0; i < N; ++i) samples[i] = 1.0f;

        ApplyExportDeclickFade(samples, N, 1, 0, N, fadeLenFrames);

        // First sample should be near 0 (fade-in start)
        CHECK(samples[0] < 0.1f, "fade-in: first sample near 0");
        // Sample at fade boundary should be near 1
        CHECK(samples[fadeLenFrames] > 0.95f, "after fade-in: near 1.0");
        // Middle samples should be untouched
        CHECK(near(samples[50], 1.0f), "middle: untouched at 1.0");
        // Last sample should be near 0 (fade-out end)
        CHECK(samples[N - 1] < 0.1f, "fade-out: last sample near 0");
        // Sample before fade-out should be near 1
        CHECK(samples[N - fadeLenFrames - 1] > 0.95f, "before fade-out: near 1.0");
    }

    // ── ApplyExportDeclickFade: stereo ──
    std::printf("\n--- ApplyExportDeclickFade stereo ---\n");
    {
        const size_t N = 20;
        const size_t channels = 2;
        float samples[N * channels];
        for (size_t i = 0; i < N * channels; ++i) samples[i] = 1.0f;

        ApplyExportDeclickFade(samples, N, channels, 0, N, 5);

        // Both channels of first frame should be faded equally
        CHECK(near(samples[0], samples[1]), "stereo: L and R faded equally at start");
        CHECK(samples[0] < 0.5f, "stereo: first frame faded");
        // Middle should be untouched
        CHECK(near(samples[10 * channels], 1.0f), "stereo: middle L untouched");
        CHECK(near(samples[10 * channels + 1], 1.0f), "stereo: middle R untouched");
    }

    // ── ApplyExportDeclickFade: null/edge cases ──
    std::printf("\n--- ApplyExportDeclickFade edge cases ---\n");
    {
        // Null pointer should not crash
        ApplyExportDeclickFade(nullptr, 100, 1, 0, 100, 10);
        CHECK(true, "null pointer: no crash");

        // Zero channels
        float buf[10] = {1.0f};
        ApplyExportDeclickFade(buf, 10, 0, 0, 10, 5);
        CHECK(true, "zero channels: no crash");

        // Zero fade length
        for (int i = 0; i < 10; ++i) buf[i] = 1.0f;
        ApplyExportDeclickFade(buf, 10, 1, 0, 10, 0);
        CHECK(near(buf[0], 1.0f), "zero fade: samples untouched");

        // Fade longer than half total -> clamped
        float buf2[10];
        for (int i = 0; i < 10; ++i) buf2[i] = 1.0f;
        ApplyExportDeclickFade(buf2, 10, 1, 0, 10, 8);
        CHECK(buf2[0] < 1.0f, "oversized fade clamped: first sample faded");
        CHECK(true, "oversized fade: no crash");
    }

    // ── ApplyExportDeclickFade: raised cosine symmetry ──
    std::printf("\n--- ApplyExportDeclickFade symmetry ---\n");
    {
        const size_t N = 40;
        const size_t fadeLen = 10;
        float samples[N];
        for (size_t i = 0; i < N; ++i) samples[i] = 1.0f;

        ApplyExportDeclickFade(samples, N, 1, 0, N, fadeLen);

        // Fade-in gain at position k should equal fade-out gain at symmetric position
        bool symmetric = true;
        for (size_t k = 0; k < fadeLen; ++k) {
            float fadeInGain = samples[k];
            float fadeOutGain = samples[N - 1 - k];
            if (!near(fadeInGain, fadeOutGain, 0.01f)) { symmetric = false; break; }
        }
        CHECK(symmetric, "raised cosine fade is symmetric");
    }

    std::printf("\n=== Utils: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
