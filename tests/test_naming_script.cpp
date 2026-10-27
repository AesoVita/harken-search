// test_naming_script.cpp — Unit tests for NamingScript (F-012)
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "../src/NamingScript.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

static std::string ReadFirstExistingFile(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) continue;
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return {};
}

int main() {
    std::printf("=== NamingScript Tests ===\n");

    NamingScriptEngine::initialize();

    // ── parseTemplate: literal only ──
    {
        auto tokens = NamingScriptEngine::parseTemplate("hello world");
        CHECK(!tokens.empty(), "parseTemplate literal: non-empty");
        bool hasLiteral = false;
        for (auto& t : tokens) if (t.type == NamingToken::Type::Literal) hasLiteral = true;
        CHECK(hasLiteral, "parseTemplate literal: has Literal token");
    }

    // ── parseTemplate: single field ──
    {
        auto tokens = NamingScriptEngine::parseTemplate("<Description>");
        bool hasField = false;
        for (auto& t : tokens) {
            if (t.type == NamingToken::Type::Field && t.text == "Description") hasField = true;
        }
        CHECK(hasField, "parseTemplate '<Description>' -> Field token");
    }

    // ── parseTemplate: field with word count ──
    {
        auto tokens = NamingScriptEngine::parseTemplate("<Description:3>");
        bool hasFieldWords = false;
        for (auto& t : tokens) {
            if (t.type == NamingToken::Type::FieldWords && t.text == "Description" && t.wordCount == 3)
                hasFieldWords = true;
        }
        CHECK(hasFieldWords, "parseTemplate '<Description:3>' -> FieldWords token");
    }

    // ── parseTemplate: mixed ──
    {
        auto tokens = NamingScriptEngine::parseTemplate("<Category>_<SubCategory>_<Description:3>");
        int fieldCount = 0;
        for (auto& t : tokens) {
            if (t.type == NamingToken::Type::Field || t.type == NamingToken::Type::FieldWords) ++fieldCount;
        }
        CHECK(fieldCount == 3, "parseTemplate mixed: 3 field tokens");
    }

    // ── applyTemplate: basic substitution ──
    std::printf("\n--- applyTemplate ---\n");
    {
        std::unordered_map<std::string, std::string> meta = {
            {"Category", "IMPACTS"},
            {"Description", "Heavy Metal Door Slam"}
        };
        std::string result = NamingScriptEngine::applyTemplate("<Category>_<Description>", meta);
        CHECK(result == "IMPACTS_Heavy Metal Door Slam", "applyTemplate basic substitution");
    }

    // ── applyTemplate: missing field → empty ──
    {
        std::unordered_map<std::string, std::string> meta = {{"Category", "AMB"}};
        std::string result = NamingScriptEngine::applyTemplate("<Category>_<Library>", meta);
        CHECK(result.find("AMB") != std::string::npos, "applyTemplate: present field substituted");
    }

    // ── applyTemplate: word truncation ──
    {
        std::unordered_map<std::string, std::string> meta = {
            {"Description", "Heavy Metal Door Slam Impact"}
        };
        std::string result = NamingScriptEngine::applyTemplate("<Description:2>", meta);
        CHECK(result == "Heavy Metal", "applyTemplate '<Description:2>' takes first 2 words");
    }

    // ── takeFirstWords ──
    std::printf("\n--- takeFirstWords ---\n");
    CHECK(NamingScriptEngine::takeFirstWords("one two three four", 2) == "one two",
          "takeFirstWords(4 words, 2) -> first 2");
    CHECK(NamingScriptEngine::takeFirstWords("single", 3) == "single",
          "takeFirstWords fewer words than N -> whole string");
    CHECK(NamingScriptEngine::takeFirstWords("", 2).empty(),
          "takeFirstWords empty -> empty");

    // ── sanitizeFilename ──
    std::printf("\n--- sanitizeFilename ---\n");
    {
        std::string clean = NamingScriptEngine::sanitizeFilename("test/file:name*here?");
        CHECK(clean.find('/') == std::string::npos, "sanitize removes /");
        CHECK(clean.find(':') == std::string::npos, "sanitize removes :");
        CHECK(clean.find('*') == std::string::npos, "sanitize removes *");
        CHECK(clean.find('?') == std::string::npos, "sanitize removes ?");
    }

    // ── sanitizeFilename: preserves good chars ──
    {
        std::string clean = NamingScriptEngine::sanitizeFilename("Normal Name 123");
        CHECK(clean == "Normal Name 123", "sanitize preserves normal chars");
    }

    // ── truncateToLength ──
    std::printf("\n--- truncateToLength ---\n");
    {
        std::string result = NamingScriptEngine::truncateToLength("abcdefghij", 5);
        CHECK(result.size() <= 5, "truncateToLength(10 chars, max 5) -> <=5 chars");
    }
    {
        std::string result = NamingScriptEngine::truncateToLength("abc", 10);
        CHECK(result == "abc", "truncateToLength short string unchanged");
    }

    // ── sanitizePath ──
    std::printf("\n--- sanitizePath ---\n");
    {
        std::string result = NamingScriptEngine::sanitizePath("C:\\Users\\test*file");
        CHECK(result.find('*') == std::string::npos, "sanitizePath removes *");
        CHECK(result.find('\\') != std::string::npos || result.find('/') != std::string::npos,
              "sanitizePath keeps path separators");
    }

    // ── applyTemplate maxLength ──
    std::printf("\n--- maxLength ---\n");
    {
        std::unordered_map<std::string, std::string> meta = {
            {"Description", "This Is A Very Long Description That Should Be Truncated At Some Point"}
        };
        std::string result = NamingScriptEngine::applyTemplate("<Description>", meta, 20);
        CHECK(result.size() <= 20, "applyTemplate respects maxLength=20");
    }

    // ── custom preset lifecycle ──
    std::printf("\n--- custom presets ---\n");
    {
        NamingPreset preset;
        preset.name = "User Custom Preset";
        preset.pattern = "<Filename>_<Category>";
        preset.maxLength = 64;
        NamingScriptEngine::addPreset(preset);
        const NamingPreset* saved = NamingScriptEngine::getPreset("User Custom Preset");
        CHECK(saved && saved->pattern == "<Filename>_<Category>",
              "addPreset persists custom preset data");

        preset.pattern = "<Filename>";
        NamingScriptEngine::addPreset(preset);
        saved = NamingScriptEngine::getPreset("User Custom Preset");
        CHECK(saved && saved->pattern == "<Filename>",
              "addPreset replaces an existing custom preset by name");

        NamingScriptEngine::removePreset("User Custom Preset");
        CHECK(NamingScriptEngine::getPreset("User Custom Preset") == nullptr,
              "removePreset deletes the custom preset");
    }

    // ── source contracts for naming workflow / batch rename ──
    std::printf("\n--- source contracts ---\n");
    {
        const std::string moduleUiSource = ReadFirstExistingFile({
            "../src/ModuleUI.cpp",
            "../../src/ModuleUI.cpp",
            "../../../src/ModuleUI.cpp",
            "src/ModuleUI.cpp"
        });
        CHECK(!moduleUiSource.empty(), "source contract can read ModuleUI.cpp");
        CHECK(moduleUiSource.find("m[\"Filename\"] = stemmedFileName;") != std::string::npos,
              "naming metadata uses the stemmed filename to avoid duplicate extensions");
        CHECK(moduleUiSource.find("m[\"Library\"] = GetExtendedNamingField(rec, \"library\");") != std::string::npos,
              "naming metadata exposes the Library field to presets");
        CHECK(moduleUiSource.find("m[\"FXName\"] = GetExtendedNamingField(rec, \"fx_name\");") != std::string::npos,
              "naming metadata exposes the FXName field to presets");
        CHECK(moduleUiSource.find("RenameFileOnDisk(state, rec.id, rec.filePath, newFileName)") != std::string::npos,
              "naming dialog routes file renames through RenameFileOnDisk for playing-file safety");
        CHECK(moduleUiSource.find("ImGui::Button(u8\"保存预设\")") != std::string::npos,
              "naming dialog exposes Chinese preset actions");
    }
    {
        const std::string mainSource = ReadFirstExistingFile({
            "../src/main.cpp",
            "../../src/main.cpp",
            "../../../src/main.cpp",
            "src/main.cpp"
        });
        CHECK(!mainSource.empty(), "source contract can read main.cpp");
        CHECK(mainSource.find("DeleteFromMatchedTextToEnd") != std::string::npos,
              "batch rename exposes the delete-from-match-to-end rule");
        CHECK(mainSource.find("result.rfind(rule.searchText)") != std::string::npos,
              "delete-from-match-to-end supports the last-match variant");
        CHECK(mainSource.find("result.find(rule.searchText)") != std::string::npos,
              "delete-from-match-to-end supports the first-match variant");
        CHECK(mainSource.find("RenameFileOnDisk(state, preview.recordId, preview.filePath, preview.newName)") != std::string::npos,
              "batch rename routes file renames through RenameFileOnDisk for playing-file safety");
        CHECK(mainSource.find("const char* occurrenceItems = u8\"首次命中\\0最后一次命中\\0\\0\";") != std::string::npos,
              "batch rename exposes Chinese occurrence labels for the delete-to-end rule");
    }

    std::printf("\n=== NamingScript: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
