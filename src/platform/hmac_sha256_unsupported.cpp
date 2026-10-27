#include "hmac_sha256.h"

namespace Platform::Crypto {

HmacSha256Result ComputeHmacSha256(const void* keyBytes,
                                   size_t keySize,
                                   const void* dataBytes,
                                   size_t dataSize) {
    HmacSha256Result result;
    if ((!keyBytes && keySize != 0) || (!dataBytes && dataSize != 0)) {
        result.status = HmacSha256Status::InvalidArgument;
        return result;
    }
    result.status = HmacSha256Status::Unsupported;
    return result;
}

} // namespace Platform::Crypto
