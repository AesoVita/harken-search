// test_ucs_naming.cpp — Unit tests for UCS Standard naming presets (F-012-ucs)
#include <cstdio>
#include <string>
#include <unordered_map>
#include "../src/NamingScript.h"

int main() {
    int failed = 0;

    NamingScriptEngine::initialize();

    // ── Test 1: "UCS Standard" preset exists ──────────────────────────────────
    const NamingPreset* ucsStd = NamingScriptEngine::getPreset("UCS Standard");
    if (!ucsStd) {
        std::printf("FAIL: 'UCS Standard' preset not found\n");
        return 1;
    }

    // ── Test 2: "UCS Compact" preset exists ───────────────────────────────────
    const NamingPreset* ucsCompact = NamingScriptEngine::getPreset("UCS Compact");
    if (!ucsCompact) {
        std::printf("FAIL: 'UCS Compact' preset not found\n");
        return 1;
    }

    // ── Test 3: UCS Standard generates correct format ─────────────────────────
    std::unordered_map<std::string, std::string> meta = {
        {"Category",    "IMPACTS"},
        {"SubCategory", "METAL"},
        {"Description", "Heavy Metal Impact Hit"},
        {"Library",     "SoundFX Pro"},
        {"Designer",    "JohnDoe"}
    };
    std::string result = NamingScriptEngine::applyTemplate(ucsStd->pattern, meta, ucsStd->maxLength);
    // Expected: IMPACTS_METAL_Heavy Metal Impact_SoundFX Pro_JohnDoe
    // (Description truncated to 3 words)
    if (result.find("IMPACTS") == std::string::npos ||
        result.find("METAL") == std::string::npos ||
        result.find("JohnDoe") == std::string::npos) {
        std::printf("FAIL: UCS Standard output wrong: '%s'\n", result.c_str());
        ++failed;
    } else {
        std::printf("PASS: UCS Standard output: '%s'\n", result.c_str());
    }

    // ── Test 4: UCS Compact uses CatID + Description + Designer ───────────────
    std::unordered_map<std::string, std::string> meta2 = {
        {"CatID",       "ImpMtl"},
        {"Description", "Metal Clang Impact"},
        {"Designer",    "JDoe"}
    };
    std::string compact = NamingScriptEngine::applyTemplate(ucsCompact->pattern, meta2, ucsCompact->maxLength);
    if (compact.find("ImpMtl") == std::string::npos || compact.find("JDoe") == std::string::npos) {
        std::printf("FAIL: UCS Compact output wrong: '%s'\n", compact.c_str());
        ++failed;
    } else {
        std::printf("PASS: UCS Compact output: '%s'\n", compact.c_str());
    }

    // ── Test 5: Sanitized output — no illegal characters ──────────────────────
    std::unordered_map<std::string, std::string> meta3 = {
        {"Category",    "IMPACTS"},
        {"SubCategory", "METAL/HEAVY"},   // slash should be removed
        {"Description", "Hit: Impact!"},
        {"Library",     "Lib"},
        {"Designer",    "Dev"}
    };
    std::string sanitized = NamingScriptEngine::applyTemplate(ucsStd->pattern, meta3, ucsStd->maxLength);
    sanitized = NamingScriptEngine::sanitizeFilename(sanitized);
    if (sanitized.find('/') != std::string::npos || sanitized.find(':') != std::string::npos) {
        std::printf("FAIL: sanitized output still has illegal chars: '%s'\n", sanitized.c_str());
        ++failed;
    } else {
        std::printf("PASS: Sanitized output: '%s'\n", sanitized.c_str());
    }

    if (failed) {
        std::printf("FAIL: %d test(s) failed\n", failed);
        return 1;
    }
    std::printf("PASS: all UCS naming tests passed\n");
    return 0;
}
