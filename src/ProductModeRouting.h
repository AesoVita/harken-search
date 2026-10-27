#pragma once

#include "AppState.h"

namespace aura {

// @stage7-contract
// API contract: TryGetBuiltInPresetProductMode synchronously inspects the
// caller-owned preset and writes outMode only when a recognized built-in
// mapping exists. It performs no global mutation, retains no references, and
// returns false for custom/unknown presets while preserving outMode.

bool TryGetBuiltInPresetProductMode(const ColumnViewPreset& preset, ProductMode& outMode);

}
