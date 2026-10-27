#pragma once

namespace Platform {

// @stage7-contract: Windows UI-thread adapter for an owned undecorated module
// window (never the main viewport). Borrows nativeWindow synchronously.
// Returns false for invalid dimensions/inputs or an OS failure. The OS owns
// the installed region; identical dimensions/radius are an idempotent no-op.
bool ApplyNativeFloatingWindowCorners(void* nativeWindow, int radius);

} // namespace Platform
