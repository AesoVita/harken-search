#pragma once

#include <string>

namespace Platform::Crypto {

// @stage7-contract
// API contract: ComputeSha256File reads the caller-owned UTF-8 path and
// returns an owned digest/status value without changing the file. It is
// synchronous and may perform blocking IO; callers must inspect status before
// using hexDigest. Failures preserve the platform error in systemError when
// available and do not throw through the platform boundary.

enum class FileHashStatus {
    Unsupported,
    Success,
    InvalidArgument,
    EncodingFailed,
    ProviderUnavailable,
    OpenFailed,
    ReadFailed,
    HashFailed,
};

struct FileHashResult {
    FileHashStatus status = FileHashStatus::Unsupported;
    std::string hexDigest;
    unsigned long systemError = 0;
};

FileHashResult ComputeSha256File(const std::string& pathUtf8);

} // namespace Platform::Crypto
