#ifdef _WIN32

#include "file_hash.h"

#include "text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <vector>

namespace Platform::Crypto {
namespace {

class ScopedHashProvider {
public:
    explicit ScopedHashProvider(BCRYPT_ALG_HANDLE handle) : handle_(handle) {}
    ~ScopedHashProvider() {
        if (handle_) BCryptCloseAlgorithmProvider(handle_, 0);
    }
    BCRYPT_ALG_HANDLE get() const { return handle_; }

private:
    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

class ScopedHash {
public:
    explicit ScopedHash(BCRYPT_HASH_HANDLE handle) : handle_(handle) {}
    ~ScopedHash() {
        if (handle_) BCryptDestroyHash(handle_);
    }
    BCRYPT_HASH_HANDLE get() const { return handle_; }

private:
    BCRYPT_HASH_HANDLE handle_ = nullptr;
};

class ScopedHashFile {
public:
    explicit ScopedHashFile(HANDLE handle) : handle_(handle) {}
    ~ScopedHashFile() {
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    }
    HANDLE get() const { return handle_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

FileHashResult Failure(FileHashStatus status, unsigned long error = 0) {
    FileHashResult result;
    result.status = status;
    result.systemError = error;
    return result;
}

} // namespace

FileHashResult ComputeSha256File(const std::string& pathUtf8) {
    if (pathUtf8.empty()) return Failure(FileHashStatus::InvalidArgument);
    const std::wstring path = TextEncoding::Utf8ToWideStrict(pathUtf8);
    if (path.empty()) return Failure(FileHashStatus::EncodingFailed);

    BCRYPT_ALG_HANDLE provider = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &provider, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) return Failure(FileHashStatus::ProviderUnavailable,
                                   static_cast<unsigned long>(status));
    ScopedHashProvider providerGuard(provider);

    DWORD objectBytes = 0;
    DWORD hashBytes = 0;
    DWORD copied = 0;
    status = BCryptGetProperty(providerGuard.get(), BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&objectBytes),
                               sizeof(objectBytes), &copied, 0);
    if (status < 0) return Failure(FileHashStatus::ProviderUnavailable,
                                   static_cast<unsigned long>(status));
    status = BCryptGetProperty(providerGuard.get(), BCRYPT_HASH_LENGTH,
                               reinterpret_cast<PUCHAR>(&hashBytes),
                               sizeof(hashBytes), &copied, 0);
    if (status < 0 || hashBytes == 0) {
        return Failure(FileHashStatus::ProviderUnavailable,
                       static_cast<unsigned long>(status));
    }
    std::vector<unsigned char> object(objectBytes);
    std::vector<unsigned char> digest(hashBytes);
    BCRYPT_HASH_HANDLE hash = nullptr;
    status = BCryptCreateHash(providerGuard.get(), &hash, object.data(), objectBytes,
                              nullptr, 0, 0);
    if (status < 0) return Failure(FileHashStatus::HashFailed,
                                   static_cast<unsigned long>(status));
    ScopedHash hashGuard(hash);

    {
        ScopedHashFile file(CreateFileW(
            path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file.get() == INVALID_HANDLE_VALUE) {
            return Failure(FileHashStatus::OpenFailed, GetLastError());
        }
        std::vector<unsigned char> buffer(64 * 1024);
        for (;;) {
            DWORD bytesRead = 0;
            if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                          &bytesRead, nullptr)) {
                return Failure(FileHashStatus::ReadFailed, GetLastError());
            }
            if (bytesRead == 0) break;
            status = BCryptHashData(hashGuard.get(), buffer.data(), bytesRead, 0);
            if (status < 0) {
                return Failure(FileHashStatus::HashFailed,
                               static_cast<unsigned long>(status));
            }
        }
    }

    status = BCryptFinishHash(hashGuard.get(), digest.data(), hashBytes, 0);
    if (status < 0) return Failure(FileHashStatus::HashFailed,
                                   static_cast<unsigned long>(status));
    static constexpr char hex[] = "0123456789abcdef";
    FileHashResult result;
    result.status = FileHashStatus::Success;
    result.hexDigest.resize(digest.size() * 2);
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result.hexDigest[index * 2] = hex[digest[index] >> 4];
        result.hexDigest[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    return result;
}

} // namespace Platform::Crypto

#endif // _WIN32
