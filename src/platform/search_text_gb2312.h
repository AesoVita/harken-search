#pragma once

#include <cstdint>

namespace Platform::SearchText {

// @stage7-contract
// API contract: Gb2312InitialForCodepoint is a pure synchronous mapping from
// a copied Unicode scalar to the existing single-byte initial result. It
// performs no IO/global mutation, retains no state, and returns the established
// fallback character when the codepoint has no supported mapping.

char Gb2312InitialForCodepoint(uint32_t codepoint);

} // namespace Platform::SearchText
