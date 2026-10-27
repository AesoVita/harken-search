#pragma once

// ProductEdition — 版本分档 + 授权/试用管理

//
// Basic, Pro, and Ultimate features are selected by edition gates.
// 运行时授权：license key → 解锁对应版本
// 试用期：首次启动起 14 天试用 Ultimate 功能

#include "BuildConfig.h"
#include <string>
#include <cstdint>
#include <vector>
#include <functional>

// @stage7-contract
// API contract: ProductEdition exposes process-global license resolution.
// State objects are caller-owned and mutated synchronously by lifecycle
// helpers; setLicenseState borrows a pointer for application lifetime and
// current/feature queries read that state without transferring ownership.
// Validation/activation failures are returned as bool or lastError and do not
// alter the public key format or edition semantics.

namespace ProductEdition {

enum class Edition : int {
    Base = 0,
    Pro = 1,
    Ultimate = 2
};

// License State — 运行时授权状态

struct LicenseState {
    // License key
    std::string licenseKey;          // XXXX-XXXX-XXXX-XXXX format
    Edition grantedEdition = Edition::Base;
    bool keyValid = false;
    std::string machineId;           // Hardware fingerprint
    int64_t activationTime = 0;      // Unix timestamp

    // Trial
    int64_t trialStartTime = 0;      // First launch timestamp (0 = not started)
    int trialDays = 14;              // Trial duration (days)
    bool trialExpired = false;

    // Derived
    Edition effectiveEdition = Edition::Base; // Computed from key + trial
    std::string lastError;
};

// License Key Format & Validation

// V1 (legacy): XXXX-XXXX-XXXX-XXXX — FNV-1a + XOR checksum (backward compat)
// V2 (current): AURA2-<base64url(sig64 + payload16)> — Ed25519 signature
// Offline-only: no server required. Private key kept in offline keygen tool.

// Validate a license key (auto-detects V1 vs V2 format)
bool validateLicenseKey(const std::string& key, const std::string& machineId,
                        Edition& grantedEdition, std::string& errorMsg);

// Generate machine ID from hardware fingerprint (Windows: MachineGuid from registry)
std::string getMachineId();

// Trial Management

// Start trial if not already started. Returns true if trial is active.
bool startTrialIfNeeded(LicenseState& state);

// Check if trial is still active. Updates trialExpired.
bool isTrialActive(const LicenseState& state);

// Get remaining trial days (0 if expired, -1 if not started)
int getTrialDaysRemaining(const LicenseState& state);

// Edition Resolution

// Resolve the effective edition from license + trial state
Edition resolveEdition(const LicenseState& state);

// Initialize license state on startup (load + validate + resolve)
void initializeLicense(LicenseState& state);

// Activate a new license key
bool activateLicense(LicenseState& state, const std::string& key);

// Deactivate current license (revert to trial or base)
void deactivateLicense(LicenseState& state);

// Runtime Edition Queries

// Get the current runtime edition (license-aware)
// If no LicenseState is set, falls back to build-time AURA_EDITION
Edition current();

// Signal that the exe binary has been tampered with (hash mismatch)
// Once set, current() always returns Base regardless of license
void setIntegrityCheckFailed();

// Set the global license state pointer (called once at startup)
void setLicenseState(LicenseState* state);

// 是否包含 Pro 及以上功能
inline bool hasPro() {
    return static_cast<int>(current()) >= static_cast<int>(Edition::Pro);
}

// Whether the current edition includes Ultimate feature gates.
inline bool hasUltimate() {
    return static_cast<int>(current()) >= static_cast<int>(Edition::Ultimate);
}

// 按名称判断
bool hasFeature(const char* featureName);

// Edition display name
const char* editionName(Edition ed);

// Data Compatibility — Upgrade/Downgrade

struct CompatibilityReport {
    bool compatible = true;
    std::vector<std::string> warnings;    // Non-blocking issues
    std::vector<std::string> lockedFeatures; // Features that will become read-only
};

// Check what happens when changing from one edition to another
CompatibilityReport checkEditionChange(Edition from, Edition to);

} // namespace ProductEdition

// 便捷宏，用于 #if 或条件编译
#define AURA_EDITION_BASE     0
#define AURA_EDITION_PRO      1
#define AURA_EDITION_ULTIMATE 2

#define AURA_HAS_PRO()       (AURA_EDITION >= AURA_EDITION_PRO)
#define AURA_HAS_ULTIMATE()  (AURA_EDITION >= AURA_EDITION_ULTIMATE)
