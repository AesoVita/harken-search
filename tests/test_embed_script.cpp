// test_embed_script.cpp — Unit tests for EmbedEngine (F-013)
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include "../src/EmbedScript.h"

static int failed = 0;
static int passed = 0;

static void CHECK(bool cond, const char* desc) {
    if (cond) { ++passed; std::printf("  PASS: %s\n", desc); }
    else      { ++failed; std::printf("  FAIL: %s\n", desc); }
}

int main() {
    std::printf("=== EmbedScript Tests ===\n");

    // ── buildCopyrightString ──
    std::printf("\n--- buildCopyrightString ---\n");
    {
        auto s = EmbedEngine::buildCopyrightString("2024", "Acme Sound", "Impact FX", "https://acme.com");
        CHECK(!s.empty(), "copyright string non-empty");
        CHECK(s.find("2024") != std::string::npos, "copyright contains year");
        CHECK(s.find("Acme Sound") != std::string::npos, "copyright contains manufacturer");
    }
    {
        auto s = EmbedEngine::buildCopyrightString("", "", "", "");
        // should not crash with empty inputs
        CHECK(true, "buildCopyrightString with empty inputs doesn't crash");
    }

    // ── validateISRC ──
    std::printf("\n--- validateISRC ---\n");
    {
        std::string cleaned;
        bool ok = EmbedEngine::validateISRC("USRC17607839", cleaned);
        CHECK(ok, "valid ISRC passes");
        CHECK(!cleaned.empty(), "cleaned ISRC non-empty");

        ok = EmbedEngine::validateISRC("", cleaned);
        CHECK(!ok, "empty ISRC fails");

        ok = EmbedEngine::validateISRC("ABC", cleaned);
        CHECK(!ok, "short ISRC fails");
    }

    // ── getField ──
    std::printf("\n--- getField ---\n");
    {
        std::unordered_map<std::string, std::string> meta = {
            {"Description", "Heavy Metal Door"},
            {"Category", "IMPACTS"}
        };

        CHECK(EmbedEngine::getField(meta, "Description") == "Heavy Metal Door", "getField existing key");
        CHECK(EmbedEngine::getField(meta, "Category") == "IMPACTS", "getField Category");
        CHECK(EmbedEngine::getField(meta, "MissingKey") == "", "getField missing returns empty");
        CHECK(EmbedEngine::getField(meta, "MissingKey", "fallback") == "fallback", "getField missing returns fallback");
    }

    // ── getDefaultConfig ──
    std::printf("\n--- getDefaultConfig ---\n");
    {
        auto cfg = EmbedEngine::getDefaultConfig();
        CHECK(cfg.embedBEXT, "default config: embedBEXT = true");
        CHECK(cfg.embedIXML, "default config: embedIXML = true");
        CHECK(cfg.embedSteinberg, "default config: embedSteinberg = true");
        CHECK(cfg.embedID3, "default config: embedID3 = true");
        CHECK(cfg.embedLISTInfo, "default config: embedLISTInfo = true");
        CHECK(cfg.embedAXML, "default config: embedAXML = true");
        CHECK(cfg.embedPicture, "default config: embedPicture = true");
        CHECK(!cfg.ixmlUserFields.empty(), "default config has iXML user fields");
    }

    // ── getDefaultMappings ──
    std::printf("\n--- getDefaultMappings ---\n");
    {
        auto mappings = EmbedEngine::getDefaultMappings();
        CHECK(!mappings.empty(), "default mappings non-empty");
        bool hasDescription = false;
        for (auto& m : mappings) {
            if (m.sourceField == "Description") hasDescription = true;
        }
        CHECK(hasDescription, "mappings include Description field");
    }

    // ── buildEmbedData ──
    std::printf("\n--- buildEmbedData ---\n");
    {
        std::unordered_map<std::string, std::string> meta = {
            {"Description", "Test Sound Effect"},
            {"Category", "FOLEY"},
            {"Designer", "John Doe"},
            {"Library", "Test Library"},
            {"Manufacturer", "TestCo"},
            {"Artist", "Test Artist"},
        };

        EmbedConfig cfg = EmbedEngine::getDefaultConfig();
        EmbedExtraParams extra;
        BEXTData bext;
        IXMLData ixml;
        SteinbergData steinberg;
        ID3EmbedData id3;
        LISTInfoData list;

        EmbedEngine::buildEmbedData(meta, "WAVE", cfg, extra, bext, ixml, steinberg, id3, list);

        CHECK(!bext.description.empty(), "BEXT description populated");
        CHECK(bext.description.size() <= 256, "BEXT description <= 256 chars");
        CHECK(!steinberg.mediaComment.empty() || !steinberg.mediaCategoryPost.empty(),
              "Steinberg data populated");
        CHECK(!list.ICMT.empty() || !list.IGNR.empty(), "LIST/INFO data populated");
    }

    std::printf("\n--- serializeIXML ---\n");
    {
        IXMLData ix;
        ix.project = "MyProject";
        ix.scene = "SC1";
        ix.take = "3";
        ix.note = "cafe <loud>";
        ix.trackList.push_back({1, "L"});
        ix.trackList.push_back({2, "R"});
        std::string xml = EmbedEngine::serializeIXML(ix);

        CHECK(xml.find("<BWFXML>") != std::string::npos, "iXML has BWFXML root");
        CHECK(xml.find("<PROJECT>MyProject</PROJECT>") != std::string::npos, "iXML PROJECT");
        CHECK(xml.find("<SCENE>SC1</SCENE>") != std::string::npos, "iXML SCENE");
        CHECK(xml.find("<TRACK_COUNT>2</TRACK_COUNT>") != std::string::npos, "iXML TRACK_COUNT==2");
        CHECK(xml.find("&lt;loud&gt;") != std::string::npos, "iXML escapes NOTE angle brackets");
        CHECK(xml.find("<NAME>L</NAME>") != std::string::npos, "iXML track NAME L");
    }

    std::printf("\n=== EmbedScript: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
