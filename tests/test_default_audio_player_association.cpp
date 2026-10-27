#include "DefaultAudioPlayerAssociation.h"
#include "platform/system.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Expect(bool cond, const char* name) {
    if (cond) {
        std::cout << "PASS: " << name << "\n";
        return;
    }
    std::cerr << "FAIL: " << name << "\n";
    ++g_failures;
}

const char kRoot[] = "Software\\HarkenSearchTest\\BUG-20260910-03\\FileExts";

std::string ExtKey(const char* ext, const char* leaf) {
    return std::string(kRoot) + "\\" + ext + "\\" + leaf;
}

void WriteProgId(const char* ext, const char* leaf) {
    Platform::WriteRegistryString(
        Platform::RegistryHive::CurrentUser, ExtKey(ext, leaf), "ProgId", "Auro.AudioFile");
}

std::string ReadProgId(const char* ext, const char* leaf) {
    const auto result = Platform::ReadRegistryString(
        Platform::RegistryHive::CurrentUser, ExtKey(ext, leaf), "ProgId");
    if (result.status != Platform::RegistryStringStatus::Success) return {};
    return result.valueUtf8;
}

void DeleteTree(const char* subkeyUtf8) {
    RegDeleteTreeA(HKEY_CURRENT_USER, subkeyUtf8);
}

bool RestrictKeyToQueryOnly(const std::string& subkeyUtf8) {
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, subkeyUtf8.c_str(), 0,
                      WRITE_DAC | READ_CONTROL, &key) != ERROR_SUCCESS) {
        return false;
    }
    EXPLICIT_ACCESSW access = {};
    access.grfAccessPermissions = KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | READ_CONTROL;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName = const_cast<wchar_t*>(L"CURRENT_USER");
    PACL acl = nullptr;
    const DWORD aclStatus = SetEntriesInAclW(1, &access, nullptr, &acl);
    bool ok = false;
    if (aclStatus == ERROR_SUCCESS && acl) {
        ok = SetSecurityInfo(key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                             nullptr, nullptr, acl, nullptr) == ERROR_SUCCESS;
        LocalFree(acl);
    }
    RegCloseKey(key);
    return ok;
}

} // namespace

int main() {
    DeleteTree("Software\\HarkenSearchTest\\BUG-20260910-03");

    WriteProgId(".wav", "UserChoice");
    WriteProgId(".wav", "UserChoiceLatest");
    WriteProgId(".mp3", "UserChoice");
    WriteProgId(".mp3", "UserChoiceLatest");
    Expect(IsRegisteredAsDefaultUnder(kRoot), "seeded UserChoice reports default");

    const bool removed = RemoveDefaultPlayerUnder(kRoot);
    Expect(removed, "remove reports success only after association is actually cleared");
    Expect(!IsRegisteredAsDefaultUnder(kRoot), "after remove, settings path must show not-default");
    Expect(ReadProgId(".wav", "UserChoice").empty(), "UserChoice ProgId gone");
    Expect(ReadProgId(".wav", "UserChoiceLatest").empty(),
           "UserChoiceLatest ProgId gone so Explorer also stops using the app");
    Expect(ReadProgId(".mp3", "UserChoiceLatest").empty(), "all seeded extensions cleared");

    DeleteTree("Software\\HarkenSearchTest\\BUG-20260910-03");
    WriteProgId(".wav", "UserChoice");
    Expect(RestrictKeyToQueryOnly(ExtKey(".wav", "UserChoice")),
           "can restrict test UserChoice to query-only");
    const bool deniedRemove = RemoveDefaultPlayerUnder(kRoot);
    Expect(!IsRegisteredAsDefaultUnder(kRoot) || !deniedRemove,
           "protected UserChoice cannot keep showing already-default after cancel");
    if (IsRegisteredAsDefaultUnder(kRoot) && deniedRemove) {
        std::cerr << "FAIL: remove pretended to succeed while ProgId still readable\n";
        ++g_failures;
    }

    DeleteTree("Software\\HarkenSearchTest\\BUG-20260910-03");

    const char kCapRoot[] = "Software\\HarkenSearchTest\\BUG-20260916-07";
    DeleteTree(kCapRoot);
    Expect(RegisterDefaultProgramsCapabilitiesUnder(kCapRoot),
           "isolated Default Programs capabilities can be written");
    const auto registered = Platform::ReadRegistryString(
        Platform::RegistryHive::CurrentUser,
        std::string(kCapRoot) + "\\Software\\RegisteredApplications",
        "HarkenSearch");
    Expect(registered.status == Platform::RegistryStringStatus::Success &&
               registered.valueUtf8 == "Software\\HarkenSearch\\Capabilities",
           "RegisteredApplications points at HarkenSearch Capabilities");
    const auto wavAssoc = Platform::ReadRegistryString(
        Platform::RegistryHive::CurrentUser,
        std::string(kCapRoot) + "\\Software\\HarkenSearch\\Capabilities\\FileAssociations",
        ".wav");
    Expect(wavAssoc.status == Platform::RegistryStringStatus::Success &&
               wavAssoc.valueUtf8 == "Auro.AudioFile",
           "Capabilities FileAssociations maps .wav to Auro.AudioFile");
    DeleteTree(kCapRoot);
    return g_failures == 0 ? 0 : 1;
}
