#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Platform::Crypto {

// @stage7-contract
// API contract: ComputeHmacSha256 synchronously borrows key/data buffers for
// the duration of the call and returns an owned fixed-size digest. Non-null
// buffers are required when their size is non-zero; callers must inspect
// status before using digest. Failure details are carried by status and the
// optional platform systemError, with no retained pointers or global state.

enum class HmacSha256Status {
    Unsupported,
    Success,
    InvalidArgument,
    ProviderUnavailable,
    HashFailed,
};

struct HmacSha256Result {
    HmacSha256Status status = HmacSha256Status::Unsupported;
    std::array<uint8_t, 32> digest{};
    unsigned long systemError = 0;
};

HmacSha256Result ComputeHmacSha256(const void* keyBytes,
                                   size_t keySize,
                                   const void* dataBytes,
                                   size_t dataSize);

} // namespace Platform::Crypto
