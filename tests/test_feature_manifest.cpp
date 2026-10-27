#include "FeatureManifest.h"
#include "platform/hmac_sha256.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << '\n';
    } else {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct TempTree {
    fs::path root;

    TempTree() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() /
               ("aris_feature_manifest_contract_" + std::to_string(nonce));
        fs::create_directories(root);
    }

    ~TempTree() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

bool writeBytes(const fs::path& path, const uint8_t* data, size_t size) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return output.good();
}

bool writeText(const fs::path& path, const std::string& text) {
    return writeBytes(path, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

std::string manifestRoot(const fs::path& path) {
    std::string result = path.string();
    result.push_back(fs::path::preferred_separator);
    return result;
}

constexpr const char kManifest[] =
    "{\"enabled_features\":[\"ai_search\",\"reaper_bridge\"]}";

// Known-answer vector for the currently compiled FeatureManifest key. An intentional
// key rotation must update this vector in the same reviewed change; do not derive it
// at runtime, because that would hide key/HMAC validation regressions.
constexpr std::array<uint8_t, 32> kValidSignature = {
    0x63, 0x2F, 0xEE, 0x6A, 0x4A, 0x51, 0xEF, 0x42,
    0x24, 0xBB, 0x2D, 0xB8, 0x11, 0x8A, 0xFB, 0x3B,
    0x49, 0x48, 0x1E, 0x00, 0xF5, 0x4A, 0x7D, 0xBF,
    0xD1, 0x15, 0x76, 0xCC, 0xC5, 0x39, 0xCD, 0x51,
};

void writeManifestPair(const fs::path& root,
                       const char* stem,
                       const uint8_t* signature,
                       size_t signatureSize) {
    check(writeText(root / (std::string(stem) + "_features.json"), kManifest),
          "manifest fixture is written");
    check(writeBytes(root / (std::string(stem) + "_features.sig"),
                     signature, signatureSize),
          "signature fixture is written");
}

void expectFallback(const fs::path& root, const char* scenario) {
    check(!FeatureManifest::loadAndVerify(manifestRoot(root)), scenario);
    check(!FeatureManifest::isLoaded(), "fallback state is not loaded");
    check(FeatureManifest::isEnabled("feature_not_in_manifest"),
          "fallback state fails open");
    check(FeatureManifest::isEnabled(nullptr), "null feature id fails open");
}

bool digestIsZero(const Platform::Crypto::HmacSha256Result& result) {
    return std::all_of(result.digest.begin(), result.digest.end(),
                       [](uint8_t byte) { return byte == 0; });
}

} // namespace

int main() {
    using Platform::Crypto::ComputeHmacSha256;
    using Platform::Crypto::HmacSha256Status;

    const auto invalidKey = ComputeHmacSha256(nullptr, 1, nullptr, 0);
    check(invalidKey.status == HmacSha256Status::InvalidArgument,
          "null key with nonzero size is invalid");
    check(digestIsZero(invalidKey), "invalid key leaves a zero digest");

    const auto invalidData = ComputeHmacSha256(nullptr, 0, nullptr, 1);
    check(invalidData.status == HmacSha256Status::InvalidArgument,
          "null data with nonzero size is invalid");
    check(digestIsZero(invalidData), "invalid data leaves a zero digest");

    const std::array<uint8_t, 20> vectorKey = {
        0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
        0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    };
    constexpr char vectorData[] = "Hi There";
    const std::array<uint8_t, 32> vectorDigest = {
        0xB0, 0x34, 0x4C, 0x61, 0xD8, 0xDB, 0x38, 0x53,
        0x5C, 0xA8, 0xAF, 0xCE, 0xAF, 0x0B, 0xF1, 0x2B,
        0x88, 0x1D, 0xC2, 0x00, 0xC9, 0x83, 0x3D, 0xA7,
        0x26, 0xE9, 0x37, 0x6C, 0x2E, 0x32, 0xCF, 0xF7,
    };
    const auto known = ComputeHmacSha256(
        vectorKey.data(), vectorKey.size(), vectorData, sizeof(vectorData) - 1);
    const auto empty = ComputeHmacSha256(nullptr, 0, nullptr, 0);
#ifdef _WIN32
    check(known.status == HmacSha256Status::Success && known.digest == vectorDigest,
          "PAL matches the RFC 4231 HMAC-SHA256 vector");
    check(empty.status == HmacSha256Status::Success && !digestIsZero(empty),
          "empty key and data are valid HMAC inputs");
#else
    check(known.status == HmacSha256Status::Unsupported && digestIsZero(known),
          "unsupported PAL leaves known-vector digest zero");
    check(empty.status == HmacSha256Status::Unsupported && digestIsZero(empty),
          "unsupported PAL leaves empty-input digest zero");
#endif

    TempTree temp;

    const fs::path missing = temp.root / "missing";
    fs::create_directories(missing);
    expectFallback(missing, "missing manifest is rejected");

    const fs::path malformed = temp.root / "malformed";
    const std::array<uint8_t, 3> shortSignature = {0x01, 0x02, 0x03};
    writeManifestPair(malformed, "aris", shortSignature.data(), shortSignature.size());
    expectFallback(malformed, "malformed signature length is rejected");

    const fs::path invalid = temp.root / "invalid";
    const std::array<uint8_t, 32> invalidSignature = {};
    writeManifestPair(invalid, "aris", invalidSignature.data(), invalidSignature.size());
    expectFallback(invalid, "invalid 32-byte signature is rejected");

    const fs::path current = temp.root / "current";
    writeManifestPair(current, "aris", kValidSignature.data(), kValidSignature.size());
#ifdef _WIN32
    check(FeatureManifest::loadAndVerify(manifestRoot(current)),
          "precomputed HMAC fixture verifies through the real implementation");
    check(FeatureManifest::isLoaded(), "verified manifest reports loaded");
    check(FeatureManifest::isEnabled("ai_search"), "first allowlisted feature is enabled");
    check(FeatureManifest::isEnabled("reaper_bridge"), "second allowlisted feature is enabled");
    check(!FeatureManifest::isEnabled("cloud_ai"), "feature outside exact allowlist is disabled");
#else
    expectFallback(current, "non-Windows HMAC remains unsupported");
#endif

    const fs::path legacy = temp.root / "legacy";
    writeManifestPair(legacy, "auro", kValidSignature.data(), kValidSignature.size());
#ifdef _WIN32
    check(FeatureManifest::loadAndVerify(manifestRoot(legacy)),
          "legacy auro manifest filename fallback verifies");
    check(FeatureManifest::isEnabled("ai_search"), "legacy manifest preserves allowlist");
#else
    expectFallback(legacy, "non-Windows legacy fallback remains unsupported");
#endif

    expectFallback(missing, "repeated load resets prior manifest state");
    check(FeatureManifest::isEnabled("cloud_ai"),
          "reset clears the prior verified feature restriction");

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "All FeatureManifest tests passed\n";
    return 0;
}
