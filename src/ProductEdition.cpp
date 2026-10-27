#include "ProductEdition.h"
#include "FeatureManifest.h"
#include "platform/system.h"
#include "ed25519/tweetnacl.h"
#include <ctime>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ProductEdition {

// Ed25519 Public Key (32 bytes) — generated offline, never changes
// Private key is NOT in this binary; kept in offline keygen tool.

static constexpr unsigned char kLicensePublicKey[32] = {
    0x23, 0x0D, 0x73, 0xB6, 0xDD, 0x55, 0x8B, 0x08,
    0xF9, 0x74, 0xE1, 0x00, 0x68, 0x81, 0x37, 0x26,
    0xEF, 0xF2, 0x6A, 0x3F, 0x6F, 0x15, 0x05, 0xF4,
    0x70, 0xA2, 0x49, 0x2B, 0xB4, 0x9A, 0xEE, 0x49,
};

// Global license state pointer (set once at startup)
static LicenseState* g_licenseState = nullptr;

// Machine ID

std::string getMachineId() {
    return Platform::GetMachineId();
}

// Simple HMAC-like Key Validation

// Key format: AAAA-BBBB-CCCC-DDDD
// AAAA = edition byte (01=Pro, 02=Ultimate) + padding
// BBBB-CCCC = machine ID hash (first 8 hex chars of simple hash)
// DDDD = checksum (XOR-fold of first 12 chars)

static uint32_t simpleHash(const std::string& input) {
    uint32_t hash = 0x811c9dc5u; // FNV-1a offset basis
    for (char c : input) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= 0x01000193u; // FNV prime
    }
    return hash;
}

static std::string toHex(uint32_t val, int digits) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0') << std::setw(digits) << val;
    return ss.str();
}

static bool parseHex(const std::string& s, uint32_t& out) {
    try {
        out = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
        return true;
    } catch (...) {
        return false;
    }
}

// Base64url decode (RFC 4648 §5, no padding)

static bool base64urlDecode(const std::string& in, std::vector<unsigned char>& out) {
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    out.clear();
    unsigned int buf = 0, bits = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        int v = T[c];
        if (v < 0) return false;
        buf = (buf << 6) | (unsigned int)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((unsigned char)(buf >> bits));
            buf &= (1u << bits) - 1;
        }
    }
    return true;
}

// V2 License Validation (Ed25519 signature)

// Key format: AURA2-<base64url(sig64 + payload16)>
// Payload: edition(1) | machineId_hash(8) | expiry_unix_le(4) | reserved(3)
static bool validateLicenseKeyV2(const std::string& key, const std::string& machineId,
                                 Edition& grantedEdition, std::string& errorMsg) {
    grantedEdition = Edition::Base;

    // Strip prefix
    std::string b64 = key.substr(6); // after "AURA2-"
    std::vector<unsigned char> raw;
    if (!base64urlDecode(b64, raw) || raw.size() != 80) {
        errorMsg = "Invalid V2 key format";
        return false;
    }

    // raw = sig(64) + payload(16)
    // TweetNaCl crypto_sign format: sig(64) || message
    // crypto_sign_open expects the combined (sig || msg) buffer
    unsigned char msg[80];
    unsigned long long msgLen = 0;
    if (crypto_sign_open(msg, &msgLen, raw.data(), 80, kLicensePublicKey) != 0) {
        errorMsg = "Invalid signature — key may be forged";
        return false;
    }
    // msgLen should be 16 (the payload)
    if (msgLen != 16) {
        errorMsg = "Invalid payload length";
        return false;
    }

    // Parse payload
    unsigned char edByte = msg[0];
    if (edByte != 0x01 && edByte != 0x02) {
        errorMsg = "Invalid edition code in key";
        return false;
    }

    // Verify machine hash (bytes 1-8)
    uint32_t machineHash = simpleHash(machineId);
    unsigned char expectedHash[8];
    for (int i = 0; i < 4; ++i) expectedHash[i] = (unsigned char)(machineHash >> (i * 8));
    // Second 4 bytes: FNV-1a of machineId reversed for extra entropy
    std::string revId(machineId.rbegin(), machineId.rend());
    uint32_t machineHash2 = simpleHash(revId);
    for (int i = 0; i < 4; ++i) expectedHash[4 + i] = (unsigned char)(machineHash2 >> (i * 8));

    if (memcmp(msg + 1, expectedHash, 8) != 0) {
        errorMsg = "License key is not valid for this machine";
        return false;
    }

    // Check expiry (bytes 9-12, little-endian uint32_t unix timestamp)
    uint32_t expiry = 0;
    for (int i = 0; i < 4; ++i) expiry |= ((uint32_t)msg[9 + i]) << (i * 8);
    if (expiry > 0) { // 0 = no expiry
        int64_t now = (int64_t)std::time(nullptr);
        if (now > (int64_t)expiry) {
            errorMsg = "License key has expired";
            return false;
        }
    }

    grantedEdition = (edByte == 0x02) ? Edition::Ultimate : Edition::Pro;
    return true;
}

// V1 Legacy License Validation (FNV-1a + XOR checksum)

static bool validateLicenseKeyV1(const std::string& key, const std::string& machineId,
                                 Edition& grantedEdition, std::string& errorMsg) {
    grantedEdition = Edition::Base;

    std::string stripped;
    for (char c : key) {
        if (c != '-') stripped += c;
    }
    if (stripped.size() != 16) {
        errorMsg = "Invalid key format (expected XXXX-XXXX-XXXX-XXXX)";
        return false;
    }

    std::string segA = stripped.substr(0, 4);
    std::string segBC = stripped.substr(4, 8);
    std::string segD = stripped.substr(12, 4);

    uint32_t edVal = 0;
    if (!parseHex(segA, edVal)) {
        errorMsg = "Invalid key format";
        return false;
    }
    int edByte = (edVal >> 8) & 0xFF;
    if (edByte != 0x01 && edByte != 0x02) {
        errorMsg = "Invalid edition code in key";
        return false;
    }

    uint32_t machineHash = simpleHash(machineId);
    std::string expectedMachineHex = toHex(machineHash, 8);
    std::string segBCUpper = segBC;
    std::transform(segBCUpper.begin(), segBCUpper.end(), segBCUpper.begin(), ::toupper);
    if (segBCUpper != expectedMachineHex) {
        errorMsg = "License key is not valid for this machine";
        return false;
    }

    uint32_t checksum = 0;
    for (int i = 0; i < 12; ++i) {
        checksum ^= static_cast<uint32_t>(static_cast<unsigned char>(stripped[i]));
        checksum = (checksum << 3) | (checksum >> 29);
    }
    std::string expectedCheck = toHex(checksum & 0xFFFF, 4);
    std::string segDUpper = segD;
    std::transform(segDUpper.begin(), segDUpper.end(), segDUpper.begin(), ::toupper);
    if (segDUpper != expectedCheck) {
        errorMsg = "Invalid license key checksum";
        return false;
    }

    grantedEdition = (edByte == 0x02) ? Edition::Ultimate : Edition::Pro;
    return true;
}

// Unified validation — auto-detects V1 vs V2

bool validateLicenseKey(const std::string& key, const std::string& machineId,
                        Edition& grantedEdition, std::string& errorMsg) {
    grantedEdition = Edition::Base;
    errorMsg.clear();
    if (key.empty()) {
        errorMsg = "Empty license key";
        return false;
    }
    // V2 keys start with "AURA2-"
    if (key.size() > 6 && key.substr(0, 6) == "AURA2-") {
        return validateLicenseKeyV2(key, machineId, grantedEdition, errorMsg);
    }
    // Legacy V1
    return validateLicenseKeyV1(key, machineId, grantedEdition, errorMsg);
}

// Trial Management

static int64_t currentUnixTime() {
    return static_cast<int64_t>(std::time(nullptr));
}

bool startTrialIfNeeded(LicenseState& state) {
    if (state.trialStartTime > 0) {
        // Trial already started
        return isTrialActive(state);
    }
    // Start trial now
    state.trialStartTime = currentUnixTime();
    state.trialExpired = false;
    return true;
}

bool isTrialActive(const LicenseState& state) {
    if (state.trialStartTime <= 0) return false;
    if (state.trialExpired) return false;
    int64_t elapsed = currentUnixTime() - state.trialStartTime;
    int64_t trialSeconds = static_cast<int64_t>(state.trialDays) * 86400;
    return elapsed < trialSeconds;
}

int getTrialDaysRemaining(const LicenseState& state) {
    if (state.trialStartTime <= 0) return -1; // Not started
    int64_t elapsed = currentUnixTime() - state.trialStartTime;
    int64_t trialSeconds = static_cast<int64_t>(state.trialDays) * 86400;
    int64_t remaining = trialSeconds - elapsed;
    if (remaining <= 0) return 0;
    return static_cast<int>((remaining + 86399) / 86400); // Ceil
}

// Shadow integrity check — forward declarations (defined in Runtime Queries section)

static uint32_t computeEditionHash(Edition ed);
static void updateEditionShadow(Edition ed);

static bool buildUnlocksEditionGates() {
#if AURA_OPEN_SOURCE_SINGLE_EDITION || AURA_BUILD_DIAGNOSTICS || AURA_INTERNAL_BETA
    return true;
#else
    return false;
#endif
}

static Edition buildEditionFallback() {
    if (buildUnlocksEditionGates()) {
        return Edition::Ultimate;
    }
    return static_cast<Edition>(AURA_EDITION);
}

// Edition Resolution

Edition resolveEdition(const LicenseState& state) {
    // QA/dev/internal-test builds need the full surface available without a
    // production license so gated workflows can be exercised end-to-end.
    if (buildUnlocksEditionGates()) {
        return Edition::Ultimate;
    }

    // 1. Valid license key takes priority
    if (state.keyValid && static_cast<int>(state.grantedEdition) > 0) {
        return state.grantedEdition;
    }
    // 2. Active trial grants Ultimate
    if (isTrialActive(state)) {
        return Edition::Ultimate;
    }
    // 3. Fall back to build-time edition
    return buildEditionFallback();
}

void initializeLicense(LicenseState& state) {
    state.machineId = getMachineId();

    // Validate stored license key if present
    if (!state.licenseKey.empty()) {
        Edition granted = Edition::Base;
        std::string err;
        if (validateLicenseKey(state.licenseKey, state.machineId, granted, err)) {
            state.keyValid = true;
            state.grantedEdition = granted;
        } else {
            state.keyValid = false;
            state.grantedEdition = Edition::Base;
            state.lastError = err;
        }
    }

    // Start trial if no valid license and trial not yet started
    if (!state.keyValid) {
        startTrialIfNeeded(state);
        // Update expired flag
        if (state.trialStartTime > 0 && !isTrialActive(state)) {
            state.trialExpired = true;
        }
    }

    // Resolve effective edition + update shadow
    state.effectiveEdition = resolveEdition(state);
    updateEditionShadow(state.effectiveEdition);
}

bool activateLicense(LicenseState& state, const std::string& key) {
    state.lastError.clear();
    state.machineId = getMachineId();

    Edition granted = Edition::Base;
    std::string err;
    if (!validateLicenseKey(key, state.machineId, granted, err)) {
        state.lastError = err;
        return false;
    }

    state.licenseKey = key;
    state.keyValid = true;
    state.grantedEdition = granted;
    state.activationTime = currentUnixTime();
    state.effectiveEdition = resolveEdition(state);
    updateEditionShadow(state.effectiveEdition);
    return true;
}

void deactivateLicense(LicenseState& state) {
    state.licenseKey.clear();
    state.keyValid = false;
    state.grantedEdition = Edition::Base;
    state.activationTime = 0;
    state.lastError.clear();

    // Re-check trial
    if (state.trialStartTime > 0 && !isTrialActive(state)) {
        state.trialExpired = true;
    }
    state.effectiveEdition = resolveEdition(state);
    updateEditionShadow(state.effectiveEdition);
}

// Runtime Edition Queries — with shadow integrity check

// Shadow hash: patching effectiveEdition alone without matching shadow → fallback to Base
static uint32_t g_editionShadow = 0;
static constexpr uint32_t kEditionSalt = 0xA7C3E1D9u;

static uint32_t computeEditionHash(Edition ed) {
    uint32_t v = static_cast<uint32_t>(ed) ^ kEditionSalt;
    v = (v * 2654435761u) >> 16; // Knuth multiplicative hash
    return v;
}

// Must be called whenever effectiveEdition changes
static void updateEditionShadow(Edition ed) {
    g_editionShadow = computeEditionHash(ed);
}

static bool g_integrityFailed = false;

void setIntegrityCheckFailed() { g_integrityFailed = true; }

void setLicenseState(LicenseState* state) {
    g_licenseState = state;
    if (state) updateEditionShadow(state->effectiveEdition);
}

Edition current() {
    // The open-source build owns the complete feature surface even when an
    // obsolete commercial sidecar reports an integrity failure.
    if (buildUnlocksEditionGates()) return Edition::Ultimate;

    if (g_integrityFailed) return Edition::Base;

    if (g_licenseState) {
        Edition ed = g_licenseState->effectiveEdition;
        // Integrity check: shadow must match, else tampered
        if (computeEditionHash(ed) != g_editionShadow) {
            return Edition::Base;
        }
        return ed;
    }
    return buildEditionFallback();
}

static bool manifestAllowsFeature(const char* featureName) {
    if (FeatureManifest::isEnabled(featureName)) return true;

    struct ManifestAlias { const char* runtimeFeature; const char* manifestFeature; };
    static const ManifestAlias aliases[] = {
        {"batch_export",      "transfer_engine"},
        {"batch_rename",      "naming_script"},
        {"drag_to_daw",       "export_workflow"},
        {"multi_library",     "multi_db"},
        {"reverse_playback",  "advanced_playback"},
        {"spectrum_analyzer", "loudness"},
        {"wwise_transfer",    "transfer_engine"},
        {"sub_playlist",      "playlists"},
    };
    for (const auto& alias : aliases) {
        if (std::strcmp(featureName, alias.runtimeFeature) == 0) {
            return FeatureManifest::isEnabled(alias.manifestFeature);
        }
    }
    return false;
}

bool hasFeature(const char* featureName) {
    if (!featureName) return false;

    Edition ed = current(); // path 1: shadow-validated

    // path 2: re-derive from license state directly (redundant cross-check)
    if (g_licenseState) {
        Edition ed2 = resolveEdition(*g_licenseState);
        if (ed != ed2) return false; // tamper detected — deny
    }

    // Feature → minimum edition mapping
    struct FeatureMap { const char* name; Edition minEdition; };
    static const FeatureMap features[] = {
        {"batch_export",       Edition::Pro},
        {"multi_library",      Edition::Pro},
        {"spectrum_analyzer",  Edition::Pro},
        {"drag_to_daw",        Edition::Pro},
        {"batch_rename",       Edition::Pro},
        {"reverse_playback",   Edition::Ultimate},
        {"cover_wall",         Edition::Ultimate},
        {"advanced_filter",    Edition::Ultimate},
        {"multi_tab",          Edition::Ultimate},
        {"custom_shortcuts",   Edition::Ultimate},
        {"workflow_engine",    Edition::Ultimate},
        {"wwise_transfer",     Edition::Pro},       // F-034: Send-to-Wwise WAAPI integration
        {"sub_playlist",       Edition::Pro},        // F-017: Sub-playlist / folder creation
    };
    for (const auto& f : features) {
        if (std::strcmp(featureName, f.name) == 0) {
            if (static_cast<int>(ed) < static_cast<int>(f.minEdition))
                return false;
            return manifestAllowsFeature(featureName);
        }
    }
    return manifestAllowsFeature(featureName);
}

const char* editionName(Edition ed) {
    switch (ed) {
        case Edition::Base:     return "Entry";
        case Edition::Pro:      return "Standard";
        case Edition::Ultimate: return "Ultimate";
    }
    return "Unknown";
}

// Data Compatibility — Upgrade/Downgrade

CompatibilityReport checkEditionChange(Edition from, Edition to) {
    CompatibilityReport report;
    report.compatible = true; // Always compatible — data is preserved

    if (static_cast<int>(to) >= static_cast<int>(from)) {
        // Upgrade or same — no issues
        return report;
    }

    // Downgrade: list features that become locked
    if (static_cast<int>(from) >= static_cast<int>(Edition::Ultimate) &&
        static_cast<int>(to) < static_cast<int>(Edition::Ultimate)) {
        report.lockedFeatures.push_back("reverse_playback");
        report.lockedFeatures.push_back("cover_wall");
        report.lockedFeatures.push_back("advanced_filter");
        report.lockedFeatures.push_back("multi_tab");
        report.lockedFeatures.push_back("custom_shortcuts");
        report.lockedFeatures.push_back("workflow_engine");
        report.warnings.push_back("Playlists created with Ultimate features will be read-only");
        report.warnings.push_back("Custom shortcuts will be preserved but inactive");
        report.warnings.push_back("Workflow presets will be preserved but not executable");
    }

    if (static_cast<int>(from) >= static_cast<int>(Edition::Pro) &&
        static_cast<int>(to) < static_cast<int>(Edition::Pro)) {
        report.lockedFeatures.push_back("batch_export");
        report.lockedFeatures.push_back("multi_library");
        report.lockedFeatures.push_back("spectrum_analyzer");
        report.lockedFeatures.push_back("drag_to_daw");
        report.lockedFeatures.push_back("batch_rename");
        report.warnings.push_back("Batch export settings will be preserved but unavailable");
        report.warnings.push_back("Additional libraries will be accessible in read-only mode");
    }

    return report;
}

} // namespace ProductEdition
