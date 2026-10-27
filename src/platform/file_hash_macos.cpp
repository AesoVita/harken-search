#ifdef __APPLE__

#include "file_hash.h"

#include <CommonCrypto/CommonDigest.h>

#include <cerrno>
#include <fcntl.h>
#include <string>
#include <vector>
#include <unistd.h>

namespace Platform::Crypto {

FileHashResult ComputeSha256File(const std::string& pathUtf8) {
    FileHashResult result;
    if (pathUtf8.empty()) {
        result.status = FileHashStatus::InvalidArgument;
        return result;
    }
    const int file = open(pathUtf8.c_str(), O_RDONLY);
    if (file < 0) {
        result.status = FileHashStatus::OpenFailed;
        result.systemError = static_cast<unsigned long>(errno);
        return result;
    }
    CC_SHA256_CTX context;
    if (CC_SHA256_Init(&context) == 0) {
        close(file);
        result.status = FileHashStatus::ProviderUnavailable;
        return result;
    }
    std::vector<unsigned char> buffer(64 * 1024);
    for (;;) {
        const ssize_t count = read(file, buffer.data(), buffer.size());
        if (count < 0) {
            const int error = errno;
            close(file);
            result.status = FileHashStatus::ReadFailed;
            result.systemError = static_cast<unsigned long>(error);
            return result;
        }
        if (count == 0) break;
        if (CC_SHA256_Update(&context, buffer.data(), static_cast<CC_LONG>(count)) == 0) {
            close(file);
            result.status = FileHashStatus::HashFailed;
            return result;
        }
    }
    close(file);
    unsigned char digest[CC_SHA256_DIGEST_LENGTH] = {};
    if (!CC_SHA256_Final(digest, &context)) {
        result.status = FileHashStatus::HashFailed;
        return result;
    }
    static constexpr char hex[] = "0123456789abcdef";
    result.status = FileHashStatus::Success;
    result.hexDigest.resize(CC_SHA256_DIGEST_LENGTH * 2);
    for (std::size_t index = 0; index < CC_SHA256_DIGEST_LENGTH; ++index) {
        result.hexDigest[index * 2] = hex[digest[index] >> 4];
        result.hexDigest[index * 2 + 1] = hex[digest[index] & 0x0f];
    }
    return result;
}

} // namespace Platform::Crypto

#endif // __APPLE__
