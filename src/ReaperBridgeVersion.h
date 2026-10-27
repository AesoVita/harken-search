#pragma once

// @stage7-contract
// API contract: this header exports compile-time bridge protocol/version
// constants only. Consumers may use them for compatibility checks and wire
// negotiation; changing values is a compatibility decision outside
// conservative Stage 7 contractization.
#define ARIS_REAPER_BRIDGE_PROTOCOL_VERSION 3
#define ARIS_REAPER_BRIDGE_VERSION_MAJOR 3
#define ARIS_REAPER_BRIDGE_VERSION_MINOR 0
#define ARIS_REAPER_BRIDGE_VERSION_PATCH 0
#define ARIS_REAPER_BRIDGE_VERSION_BUILD 0
#define ARIS_REAPER_BRIDGE_VERSION_STRING "3.0.0.0"
#define ARIS_REAPER_BRIDGE_MANIFEST "aris-reaper-bridge-v3-placement-transaction"
#if defined(_M_X64) || defined(_WIN64) || defined(__x86_64__)
#define ARIS_REAPER_BRIDGE_ARCHITECTURE "x64"
#elif defined(_M_IX86) || defined(__i386__)
#define ARIS_REAPER_BRIDGE_ARCHITECTURE "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
#define ARIS_REAPER_BRIDGE_ARCHITECTURE "arm64"
#else
#define ARIS_REAPER_BRIDGE_ARCHITECTURE "unknown"
#endif
