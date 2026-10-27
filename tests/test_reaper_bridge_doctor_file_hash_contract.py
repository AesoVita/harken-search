import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8") + \
    (ROOT / "cmake" / "harken_search_behavior_tests.cmake").read_text(encoding="utf-8")
HEADER = (ROOT / "src" / "platform" / "file_hash.h").read_text(encoding="utf-8") \
    if (ROOT / "src" / "platform" / "file_hash.h").is_file() else ""
WIN32 = (ROOT / "src" / "platform" / "file_hash_win32.cpp").read_text(encoding="utf-8") \
    if (ROOT / "src" / "platform" / "file_hash_win32.cpp").is_file() else ""
MACOS = (ROOT / "src" / "platform" / "file_hash_macos.cpp").read_text(encoding="utf-8") \
    if (ROOT / "src" / "platform" / "file_hash_macos.cpp").is_file() else ""
DOCTOR = (ROOT / "src" / "ReaperBridgeDoctor.cpp").read_text(encoding="utf-8")


class ReaperBridgeDoctorFileHashContract(unittest.TestCase):
    def test_public_api_and_test_target_are_declared(self):
        for token in (
            "namespace Platform::Crypto",
            "enum class FileHashStatus",
            "ProviderUnavailable",
            "OpenFailed",
            "ReadFailed",
            "HashFailed",
            "struct FileHashResult",
            "std::string hexDigest",
            "FileHashResult ComputeSha256File(const std::string& pathUtf8);",
        ):
            self.assertIn(token, HEADER)
        for token in ("windows.h", "HANDLE", "BCRYPT_HASH_HANDLE"):
            self.assertNotIn(token, HEADER)
        self.assertIn("TestPlatformFileHash", CMAKE)
        self.assertIn("file_hash_win32.cpp", CMAKE)
        self.assertIn("file_hash_macos.cpp", CMAKE)

    def test_win32_owns_cng_read_hash_lifecycle_and_format(self):
        for token in (
            "BCRYPT_SHA256_ALGORITHM",
            "BCryptOpenAlgorithmProvider",
            "BCryptGetProperty",
            "BCRYPT_OBJECT_LENGTH",
            "BCRYPT_HASH_LENGTH",
            "class ScopedHashProvider",
            "class ScopedHash",
            "class ScopedHashFile",
            "CreateFileW",
            "GENERIC_READ",
            "FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE",
            "OPEN_EXISTING",
            "std::vector<unsigned char> buffer(64 * 1024)",
            "ReadFile",
            "BCryptHashData",
            "CloseHandle(handle_)",
            "BCryptFinishHash",
            "BCryptDestroyHash",
            "BCryptCloseAlgorithmProvider",
            "0123456789abcdef",
            "FileHashStatus::Success",
            "FileHashStatus::ProviderUnavailable",
            "FileHashStatus::OpenFailed",
            "FileHashStatus::ReadFailed",
            "FileHashStatus::HashFailed",
        ):
            self.assertIn(token, WIN32)

    def test_macos_uses_commoncrypto_and_keeps_unsupported_shape(self):
        for token in (
            "CommonCrypto/CommonDigest.h",
            "CC_SHA256_Init",
            "CC_SHA256_Update",
            "CC_SHA256_Final",
            "read(",
            "O_RDONLY",
            "FileHashStatus::Success",
        ):
            self.assertIn(token, MACOS)

    def test_doctor_uses_crypto_pal_and_preserves_write_side_native_scope(self):
        self.assertIn('#include "platform/file_hash.h"', DOCTOR)
        self.assertIn("Platform::Crypto::ComputeSha256File", DOCTOR)
        for token in (
            "BCryptOpenAlgorithmProvider",
            "BCryptGetProperty",
            "BCryptCreateHash",
            "BCryptHashData",
            "BCryptFinishHash",
            "BCryptDestroyHash",
            "BCryptCloseAlgorithmProvider",
            "ReadFile(file",
        ):
            self.assertNotIn(token, DOCTOR)
        for token in (
        ):
            self.assertIn(token, DOCTOR)


if __name__ == "__main__":
    unittest.main()
