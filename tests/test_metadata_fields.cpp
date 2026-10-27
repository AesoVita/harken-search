// test_metadata_fields.cpp — Unit tests for MetadataFieldManager (F-006)
#include <cstdio>
#include <string>
#include <vector>
#include "../src/MetadataFields.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== MetadataFields Tests ===\n");

    MetadataFieldManager::initialize();

// Initialization
    std::printf("\n--- Initialization ---\n");
    CHECK(MetadataFieldManager::isInitialized(), "isInitialized after initialize()");
    CHECK(MetadataFieldManager::getFieldCount() > 30, "fieldCount > 30 (expect ~80+ fields)");

// getAllFields
    std::printf("\n--- getAllFields ---\n");
    {
        auto& fields = MetadataFieldManager::getAllFields();
        CHECK(!fields.empty(), "getAllFields non-empty");
        CHECK((int)fields.size() == MetadataFieldManager::getFieldCount(), "getAllFields.size == getFieldCount");
    }

// getField by name
    std::printf("\n--- getField ---\n");
    {
        auto* f = MetadataFieldManager::getField("Description");
        CHECK(f != nullptr, "getField('Description') found");
        if (f) {
            CHECK(f->name == "Description", "field name matches");
            CHECK(!f->displayName.empty(), "displayName non-empty");
            CHECK(!f->displayName_zh.empty(), "displayName_zh non-empty");
            CHECK(f->type == FieldType::String, "Description is String type");
            CHECK(f->maxLength > 0, "maxLength > 0");
        }

        auto* missing = MetadataFieldManager::getField("NoSuchField_XYZ");
        CHECK(missing == nullptr, "getField unknown returns nullptr");
    }

// Known fields exist
    std::printf("\n--- Known fields ---\n");
    {
        const char* expected[] = {
            "Description", "Category", "SubCategory", "Library",
            "Designer", "Artist", "Keywords"
        };
        for (auto name : expected) {
            auto* f = MetadataFieldManager::getField(name);
            CHECK(f != nullptr, (std::string("field '") + name + "' exists").c_str());
        }
    }

// getFieldsByGroup
    std::printf("\n--- getFieldsByGroup ---\n");
    {
        auto basic = MetadataFieldManager::getFieldsByGroup("Basic");
        CHECK(!basic.empty(), "getFieldsByGroup('Basic') non-empty");

        auto empty = MetadataFieldManager::getFieldsByGroup("NonExistentGroup_XYZ");
        CHECK(empty.empty(), "getFieldsByGroup unknown returns empty");
    }

// getStringFieldNames
    std::printf("\n--- getStringFieldNames ---\n");
    {
        auto names = MetadataFieldManager::getStringFieldNames();
        CHECK(!names.empty(), "getStringFieldNames non-empty");
        bool hasDesc = false;
        for (auto& n : names) if (n == "Description") hasDesc = true;
        CHECK(hasDesc, "getStringFieldNames includes 'Description'");
    }

// getFieldLengthTable
    std::printf("\n--- getFieldLengthTable ---\n");
    {
        auto& table = MetadataFieldManager::getFieldLengthTable();
        CHECK(!table.empty(), "fieldLengthTable non-empty");
        auto it = table.find("Description");
        CHECK(it != table.end(), "fieldLengthTable has 'Description'");
        if (it != table.end()) {
            CHECK(it->second > 0, "Description maxLength > 0");
        }
    }

// Templates
    std::printf("\n--- Templates ---\n");
    {
        auto* universal = MetadataFieldManager::getTemplate(DatabaseTemplate::Universal);
        CHECK(universal != nullptr, "Universal template exists");
        if (universal) {
            CHECK(!universal->fieldNames.empty(), "Universal template has fieldNames");
            CHECK(!universal->name.empty(), "Universal template has a name");
        }

        auto* sfx = MetadataFieldManager::getTemplate(DatabaseTemplate::SoundEffects);
        CHECK(sfx != nullptr, "SoundEffects template exists");

        auto* music = MetadataFieldManager::getTemplate(DatabaseTemplate::Music);
        CHECK(music != nullptr, "Music template exists");

        auto types = MetadataFieldManager::getAllTemplateTypes();
        CHECK(types.size() >= 4, "at least 4 template types");
    }

// getTemplateName
    std::printf("\n--- getTemplateName ---\n");
    {
        auto name = MetadataFieldManager::getTemplateName(DatabaseTemplate::Universal);
        CHECK(!name.empty(), "getTemplateName(Universal) non-empty");
    }

// validateISRC
    std::printf("\n--- validateISRC ---\n");
    {
        auto r1 = MetadataFieldManager::validateISRC("USRC17607839");
        CHECK(r1.valid, "valid ISRC: USRC17607839");
        CHECK(r1.cleaned == "USRC17607839", "cleaned ISRC matches");

        auto r2 = MetadataFieldManager::validateISRC("US-RC1-76-07839");
        CHECK(r2.valid, "valid ISRC with hyphens");
        CHECK(r2.cleaned.find('-') == std::string::npos, "cleaned ISRC has no hyphens");

        auto r3 = MetadataFieldManager::validateISRC("");
        CHECK(!r3.valid, "empty ISRC invalid");

        auto r4 = MetadataFieldManager::validateISRC("ABC");
        CHECK(!r4.valid, "too short ISRC invalid");

        auto r5 = MetadataFieldManager::validateISRC("USRC1760783!");
        CHECK(!r5.valid, "ISRC with special char invalid");
    }

// validateBPM
    std::printf("\n--- validateBPM ---\n");
    {
        CHECK(MetadataFieldManager::validateBPM("120"), "BPM '120' valid");
        CHECK(MetadataFieldManager::validateBPM("120.5"), "BPM '120.5' valid");
        CHECK(MetadataFieldManager::validateBPM("60"), "BPM '60' valid");
        CHECK(MetadataFieldManager::validateBPM(""), "empty BPM valid (pass-through)");
        CHECK(!MetadataFieldManager::validateBPM("abc"), "alpha BPM invalid");
        CHECK(!MetadataFieldManager::validateBPM("0"), "BPM '0' invalid");
        CHECK(!MetadataFieldManager::validateBPM("-10"), "negative BPM invalid");
    }

// validateYear
    std::printf("\n--- validateYear ---\n");
    {
        CHECK(MetadataFieldManager::validateYear("2024"), "year '2024' valid");
        CHECK(MetadataFieldManager::validateYear("1990"), "year '1990' valid");
        CHECK(MetadataFieldManager::validateYear(""), "empty year valid (pass-through)");
        CHECK(!MetadataFieldManager::validateYear("abc"), "alpha year invalid");
        CHECK(!MetadataFieldManager::validateYear("99"), "2-digit year invalid");
    }

    std::printf("\n=== MetadataFields: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
