#include "hmac_sha256.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <limits>

namespace Platform::Crypto {
namespace {

class AlgorithmHandle {
public:
    ~AlgorithmHandle() {
        if (value_) BCryptCloseAlgorithmProvider(value_, 0);
    }

    BCRYPT_ALG_HANDLE* Put() { return &value_; }
    BCRYPT_ALG_HANDLE Get() const { return value_; }

    AlgorithmHandle() = default;
    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;

private:
    BCRYPT_ALG_HANDLE value_ = nullptr;
};

class HashHandle {
public:
    ~HashHandle() {
        if (value_) BCryptDestroyHash(value_);
    }

    BCRYPT_HASH_HANDLE* Put() { return &value_; }
    BCRYPT_HASH_HANDLE Get() const { return value_; }

    HashHandle() = default;
    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;

private:
    BCRYPT_HASH_HANDLE value_ = nullptr;
};

unsigned long ErrorBits(NTSTATUS status) {
    return static_cast<unsigned long>(status);
}

bool ExceedsNativeSize(size_t size) {
    return size > static_cast<size_t>(std::numeric_limits<ULONG>::max());
}

} // namespace

HmacSha256Result ComputeHmacSha256(const void* keyBytes,
                                   size_t keySize,
                                   const void* dataBytes,
                                   size_t dataSize) {
    HmacSha256Result result;
    if ((!keyBytes && keySize != 0) || (!dataBytes && dataSize != 0) ||
        ExceedsNativeSize(keySize) || ExceedsNativeSize(dataSize)) {
        result.status = HmacSha256Status::InvalidArgument;
        return result;
    }

    AlgorithmHandle algorithm;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        algorithm.Put(), BCRYPT_SHA256_ALGORITHM, nullptr,
        BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) {
        result.status = HmacSha256Status::ProviderUnavailable;
        result.systemError = ErrorBits(status);
        return result;
    }

    HashHandle hash;
    status = BCryptCreateHash(
        algorithm.Get(), hash.Put(), nullptr, 0,
        const_cast<PUCHAR>(static_cast<const UCHAR*>(keyBytes)),
        static_cast<ULONG>(keySize), 0);
    if (!BCRYPT_SUCCESS(status)) {
        result.status = HmacSha256Status::HashFailed;
        result.systemError = ErrorBits(status);
        return result;
    }

    if (dataSize != 0) {
        status = BCryptHashData(
            hash.Get(), const_cast<PUCHAR>(static_cast<const UCHAR*>(dataBytes)),
            static_cast<ULONG>(dataSize), 0);
        if (!BCRYPT_SUCCESS(status)) {
            result.status = HmacSha256Status::HashFailed;
            result.systemError = ErrorBits(status);
            return result;
        }
    }

    std::array<uint8_t, 32> digest{};
    status = BCryptFinishHash(hash.Get(), digest.data(),
                              static_cast<ULONG>(digest.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        result.status = HmacSha256Status::HashFailed;
        result.systemError = ErrorBits(status);
        return result;
    }

    result.status = HmacSha256Status::Success;
    result.digest = digest;
    return result;
}

} // namespace Platform::Crypto
