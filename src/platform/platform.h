#pragma once

// Platform Abstraction Layer — Aris cross-platform support (F-031)
//
// All platform-specific code should go through these interfaces.
// Implementations: platform_win32.cpp (Windows), platform_macos.mm (macOS)

// @stage7-contract
// API contract: this header is the stable platform-boundary marker and
// intentionally exports no declarations. Platform implementations remain
// behind their dedicated headers; consumers must not include native headers or
// depend on implementation-specific symbols through this boundary.
