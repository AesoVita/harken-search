#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Platform::QaFiles {

// @stage7-contract
// API contract: ReadCommandJson fills caller-owned outJson up to maxBytes;
// WriteResultAtomically borrows path/string_view arguments for the call and
// commits the supplied JSON through the existing temporary-path protocol.
// Both are synchronous, retain no references, and return false on validation
// or filesystem failure without throwing across the QA boundary.

bool ReadCommandJson(const std::string& pathUtf8,
                     std::string& outJson,
                     size_t maxBytes);
bool WriteResultAtomically(const std::string& resultPathUtf8,
                           const std::string& temporaryPathUtf8,
                           std::string_view json);

#ifdef ARIS_QA_FILES_TESTING
namespace Testing { void RemoveBeforeRetirementOnce(); }
#endif

} // namespace Platform::QaFiles
