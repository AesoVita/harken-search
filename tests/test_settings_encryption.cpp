#include "src/Utils.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

static int g_failed = 0;

static void Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        ++g_failed;
        return;
    }
    std::printf("PASS: %s\n", message);
}

static bool StartsWith(const std::string& value, const char* prefix) {
    const std::string p(prefix);
    return value.size() >= p.size() && value.compare(0, p.size(), p) == 0;
}

static std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    bool sourceContractOnly = false;
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        const char* argument = argv[argumentIndex];
        if (argument != nullptr && std::string(argument) == "--source-contract-only") {
            sourceContractOnly = true;
        }
    }
    if (sourceContractOnly) {
        const auto mainPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "src" / "main.cpp";
        if (!std::filesystem::is_regular_file(mainPath)) {
            Check(false, "main.cpp source is readable");
            return 1;
        }
        std::ifstream mainSource(mainPath, std::ios::binary);
        const std::string mainText((std::istreambuf_iterator<char>(mainSource)), std::istreambuf_iterator<char>());
        Check(mainText.find("##SiliconFlowApiKey") == std::string::npos &&
                  mainText.find("g_siliconFlowDialogKeyBuf") == std::string::npos,
              "settings page no longer renders a SiliconFlow API key input");
        Check(mainText.find("StoreSiliconFlowApiKey(state.settings.siliconFlowApiKey)") == std::string::npos,
              "settings save no longer stores a new SiliconFlow API key");
        Check(mainText.find("ProtectSettingsValue") != std::string::npos,
              "settings save uses observable DPAPI protection");
        return g_failed == 0 ? 0 : 1;
    }

    const std::string plain = "sk-test-placeholder-value-123456";
    const std::string encrypted = EncryptSettingsValue(plain);

    Check(StartsWith(encrypted, "ENC:"), "encrypted settings value uses ENC prefix");
    Check(encrypted.find(plain) == std::string::npos, "encrypted settings value does not contain plaintext");
    Check(DecryptSettingsValue(encrypted) == plain, "encrypted settings value round-trips");
    Check(DecryptSettingsValue(plain) == plain, "plaintext settings value remains migration-compatible");
    Check(EncryptSettingsValue("").empty(), "empty settings value stays empty");
    Check(DecryptSettingsValue("").empty(), "empty encrypted settings value stays empty");
    Check(IsValidSiliconFlowApiKey(plain), "SiliconFlow API key accepts the expected sk- form");
    Check(!IsValidSiliconFlowApiKey("garbled-after-machine-change"), "invalid legacy key is rejected before migration");
    Check(!IsValidSiliconFlowApiKey(""), "empty legacy key is rejected before migration");

    const auto protectedValue = ProtectSettingsValue(plain);
    Check(protectedValue.success, "DPAPI protection succeeds");
    Check(StartsWith(protectedValue.encodedValue, "DPAPI1:"), "protected value uses DPAPI1 prefix");
    Check(protectedValue.encodedValue.find(plain) == std::string::npos, "protected value excludes plaintext");
    const auto restored = UnprotectSettingsValue(protectedValue.encodedValue);
    Check(restored.success && restored.plaintext == plain, "DPAPI value round-trips");
    Check(ProtectSettingsValue("").success && ProtectSettingsValue("").encodedValue.empty(),
          "empty value stays empty");
    Check(UnprotectSettingsValue("").success && UnprotectSettingsValue("").plaintext.empty(),
          "empty protected value stays empty");

    {
        ScopedSettingsProtectionProvider provider(
            [](const std::string&) { return SettingsProtectionResult::Failure("protect_failed"); });
        const auto failed = ProtectSettingsValue(plain);
        Check(!failed.success && failed.errorKind == "protect_failed", "DPAPI failure is observable");
    }

    const auto malformed = UnprotectSettingsValue("DPAPI1:not-hex");
    Check(!malformed.success && malformed.errorKind == "format_invalid", "malformed DPAPI value is rejected");
    const auto migrated = UnprotectSettingsValue(encrypted);
    Check(migrated.success && migrated.plaintext == plain, "legacy ENC value remains readable");

#ifdef _WIN32
    const std::string testTarget = "Aris/SiliconFlowApiKeyTest";
    Check(DeleteWindowsCredential(testTarget), "isolated test target starts clean");
    Check(StoreWindowsCredential(testTarget, plain), "test key writes to isolated Windows secure storage");
    std::string loaded;
    Check(LoadWindowsCredential(testTarget, loaded), "test key reads from isolated Windows secure storage");
    Check(loaded == plain, "isolated secure storage round-trips");
    Check(DeleteWindowsCredential(testTarget), "isolated secure storage deletes the key");
    loaded.clear();
    Check(!LoadWindowsCredential(testTarget, loaded), "deleted isolated key is no longer readable");

    const std::wstring emptyTarget = L"Aris/EmptyCredentialTest";
    CREDENTIALW emptyCredential{};
    emptyCredential.Type = CRED_TYPE_GENERIC;
    emptyCredential.TargetName = const_cast<LPWSTR>(emptyTarget.c_str());
    emptyCredential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    emptyCredential.CredentialBlobSize = 0;
    emptyCredential.CredentialBlob = nullptr;
    CredDeleteW(emptyTarget.c_str(), CRED_TYPE_GENERIC, 0);
    Check(CredWriteW(&emptyCredential, 0) == TRUE, "empty credential fixture writes");
    loaded.clear();
    Check(!LoadWindowsCredential("Aris/EmptyCredentialTest", loaded), "empty credential is treated as missing");
    Check(DeleteWindowsCredential("Aris/EmptyCredentialTest"), "empty credential fixture deletes");

    const std::string txnTarget = "Aris/SiliconFlowApiKeyTestTxn";
    const std::string oldKey = "sk-old-saved-placeholder-aaaaaaaa";
    const std::string newKey = "sk-new-unsaved-placeholder-bbbbbbbb";
    Check(DeleteWindowsCredential(txnTarget), "transactional test target starts clean");
    Check(StoreWindowsCredential(txnTarget, oldKey), "transactional fixture stores old key");
    const auto tempDir = std::filesystem::temp_directory_path() /
                         "aris-bug-20260817-02-settings-encryption";
    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);
    const auto settingsPath = tempDir / "settings.ini";
    {
        std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
        out << "siliconFlowApiKey=" << oldKey << "\n";
    }
    const std::string before = ReadAll(settingsPath);
    std::string inputBuffer = newKey;
    {
        ScopedSettingsProtectionProvider provider(
            [](const std::string&) { return SettingsProtectionResult::Failure("protect_failed"); });
        const auto result = SaveSiliconFlowApiKeyTransactional(
            settingsPath.u8string(), txnTarget, oldKey, inputBuffer);
        Check(!result.success && result.errorKind == "protect_failed",
              "failed protection rejects save");
        Check(result.retainedInput == newKey, "failed protection retains the typed input");
        Check(result.savedPlaintext == oldKey, "failed protection keeps the previously saved key");
    }
    Check(ReadAll(settingsPath) == before, "failed protection keeps old settings file");
    Check(ReadAll(settingsPath).find(newKey) == std::string::npos,
          "new key never reaches settings file");
    loaded.clear();
    Check(LoadWindowsCredential(txnTarget, loaded) && loaded == oldKey,
          "failed protection keeps old isolated credential");
    Check(inputBuffer == newKey, "caller input buffer is left unchanged");
    Check(DeleteWindowsCredential(txnTarget), "transactional test target deletes");
    std::filesystem::remove_all(tempDir, ec);
#endif

    const auto mainPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "src" / "main.cpp";
    std::ifstream mainSource(mainPath, std::ios::binary);
    const std::string mainText((std::istreambuf_iterator<char>(mainSource)), std::istreambuf_iterator<char>());
    Check(mainText.find("##SiliconFlowApiKey") == std::string::npos &&
              mainText.find("g_siliconFlowDialogKeyBuf") == std::string::npos,
          "settings page no longer renders a SiliconFlow API key input");
    Check(mainText.find("StoreSiliconFlowApiKey(state.settings.siliconFlowApiKey)") == std::string::npos,
          "settings save no longer stores a new SiliconFlow API key");
    Check(mainText.find("ProtectSettingsValue") != std::string::npos,
          "settings save uses observable DPAPI protection");

    return g_failed == 0 ? 0 : 1;
}
