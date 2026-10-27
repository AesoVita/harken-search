// Standalone utility tests do not access the real credential manager or DPAPI.
#include "platform/system.h"

namespace Platform {
std::wstring LegacyHardwareFingerprint() { return L"isolated-utils-test"; }
bool ProtectUserData(std::string_view, std::string& bytes) {
    bytes.clear();
    return false;
}
bool UnprotectUserData(std::string_view, std::string& plaintext) {
    plaintext.clear();
    return false;
}
bool StoreGenericCredential(const std::string&, std::string_view) { return false; }
bool LoadGenericCredential(const std::string&, std::string& value) {
    value.clear();
    return false;
}
bool DeleteGenericCredential(const std::string&) { return false; }
} // namespace Platform
