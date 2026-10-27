// test_path_utils.cpp — Unit tests for HarkenSearch::PathUtils
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/PathUtils.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== PathUtils Tests ===\n");

    // ── CanonicalFolderPath: empty ──
    std::printf("\n--- CanonicalFolderPath empty ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("");
        CHECK(r.empty(), "empty input -> empty");
    }

    // ── CanonicalFolderPath: separator normalization ──
    std::printf("\n--- CanonicalFolderPath separators ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("E:/Sounds/FX/Impacts");
        CHECK(r.find('/') == std::string::npos, "forward slashes normalized to backslash");
        CHECK(r == "e:\\sounds\\fx\\impacts", "full normalization: lowercase + backslash");
    }

    // ── CanonicalFolderPath: long path prefix stripping ──
    std::printf("\n--- CanonicalFolderPath long path prefix ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("\\\\?\\E:\\Sounds\\FX");
        CHECK(r == "e:\\sounds\\fx", "\\\\?\\ prefix stripped");

        auto r2 = HarkenSearch::CanonicalFolderPath("\\\\?\\UNC\\server\\share");
        CHECK(r2.find("\\\\?\\") == std::string::npos, "\\\\?\\UNC\\ prefix stripped");
    }

    // ── CanonicalFolderPath: trailing slash removal ──
    std::printf("\n--- CanonicalFolderPath trailing slash ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("E:\\Sounds\\FX\\");
        CHECK(r == "e:\\sounds\\fx", "trailing backslash removed");

        auto r2 = HarkenSearch::CanonicalFolderPath("E:\\Sounds\\FX\\\\");
        CHECK(r2 == "e:\\sounds\\fx", "multiple trailing slashes removed");
    }

    // ── CanonicalFolderPath: drive root preserved ──
    std::printf("\n--- CanonicalFolderPath drive root ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("E:\\");
        CHECK(r == "e:\\", "drive root E:\\ preserved");

        auto r2 = HarkenSearch::CanonicalFolderPath("C:\\");
        CHECK(r2 == "c:\\", "drive root C:\\ preserved");
    }

    // ── CanonicalFolderPath: lowercase ──
    std::printf("\n--- CanonicalFolderPath lowercase ---\n");
    {
        auto r = HarkenSearch::CanonicalFolderPath("E:\\MiXeD\\CaSe\\PATH");
        CHECK(r == "e:\\mixed\\case\\path", "ASCII lowercase applied");
    }

    // ── CanonicalFolderPath: idempotency ──
    std::printf("\n--- CanonicalFolderPath idempotency ---\n");
    {
        auto r1 = HarkenSearch::CanonicalFolderPath("E:/Sounds/FX/");
        auto r2 = HarkenSearch::CanonicalFolderPath(r1);
        CHECK(r1 == r2, "double-canonicalize is idempotent");
    }

    // ── DisplayFolderFromFilePath ──
    std::printf("\n--- DisplayFolderFromFilePath ---\n");
    {
        auto r = HarkenSearch::DisplayFolderFromFilePath("E:\\Sounds\\FX\\boom.wav");
        // Should return the folder portion, preserving original case
        CHECK(!r.empty(), "non-empty for valid file path");
        CHECK(r.find("boom.wav") == std::string::npos, "filename stripped");

        auto r2 = HarkenSearch::DisplayFolderFromFilePath("");
        CHECK(r2.empty(), "empty path -> empty");
    }

    // ── DisplayFolderFromFilePath: preserves case ──
    std::printf("\n--- DisplayFolderFromFilePath case preservation ---\n");
    {
        auto r = HarkenSearch::DisplayFolderFromFilePath("E:\\MyLibrary\\SubFolder\\file.wav");
        // Case should be preserved (unlike CanonicalFolderPath)
        bool hasUpper = (r.find('M') != std::string::npos || r.find('S') != std::string::npos);
        CHECK(hasUpper, "case preserved in display path");
    }

    // ── ResolveFolderBreadcrumbDisplayLabel ──
    std::printf("\n--- ResolveFolderBreadcrumbDisplayLabel ---\n");
    {
        auto r = HarkenSearch::ResolveFolderBreadcrumbDisplayLabel("E:\\Sounds\\FX", "All Files");
        CHECK(!r.empty(), "non-empty for valid path");

        auto r2 = HarkenSearch::ResolveFolderBreadcrumbDisplayLabel("", "All Files");
        CHECK(r2 == "All Files", "empty path -> fallback label");
    }

    // ── ResolveActiveFolderBreadcrumbFilterPath ──
    std::printf("\n--- ResolveActiveFolderBreadcrumbFilterPath ---\n");
    {
        auto r = HarkenSearch::ResolveActiveFolderBreadcrumbFilterPath(true, "E:\\Sounds");
        CHECK(r == "E:\\Sounds", "active filter -> selected path");

        auto r2 = HarkenSearch::ResolveActiveFolderBreadcrumbFilterPath(false, "E:\\Sounds");
        CHECK(r2.empty(), "inactive filter -> empty");
    }

    // ── ResolveFolderBreadcrumbDisplayPath ──
    std::printf("\n--- ResolveFolderBreadcrumbDisplayPath ---\n");
    {
        auto r = HarkenSearch::ResolveFolderBreadcrumbDisplayPath(
            "E:\\Sounds\\FX\\boom.wav", "E:\\Sounds\\FX", "");
        CHECK(!r.empty(), "non-empty with valid file path");

        auto r2 = HarkenSearch::ResolveFolderBreadcrumbDisplayPath("", "", "E:\\Sounds\\FX");
        CHECK(!r2.empty(), "active filter path used as fallback");
    }

    std::printf("\n=== PathUtils: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
