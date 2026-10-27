#pragma once

#include <string>

namespace Platform::ProTools::PathEncoding {

// @stage7-contract
// API contract: ToImportPath synchronously converts the caller-owned UTF-8
// path to the host import representation and returns an owned string. It
// retains no references, performs no filesystem access, and preserves the
// existing empty/unchanged behavior for invalid or unsupported input.

std::string ToImportPath(const std::string& utf8Path);

} // namespace Platform::ProTools::PathEncoding
