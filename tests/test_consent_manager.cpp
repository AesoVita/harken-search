#include "src/ConsentManager.h"
#include "src/platform/filesystem.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

static_assert(std::string_view(aris::legal::kCurrentEulaVersion) == "1");
static_assert(std::string_view(aris::legal::kCurrentPrivacyVersion) == "1");

int main() {
    std::error_code ec;
    const auto legalRoot = std::filesystem::temp_directory_path() / "aris-consent-legal-documents";
    std::filesystem::remove_all(legalRoot, ec);
    std::filesystem::create_directories(legalRoot, ec);
    if (!Platform::FileSystem::WriteFileText(
            (legalRoot / "EULA.md").string(), "> EULA TEST BODY\n唯一 EULA 内容\n") ||
        !Platform::FileSystem::WriteFileText(
            (legalRoot / "PRIVACY_POLICY.md").string(), "> PRIVACY TEST BODY\n唯一隐私内容\n")) {
        std::printf("FAIL: legal document fixtures should be writable\n");
        return 1;
    }
    aris::legal::LegalDocuments legalDocuments;
    if (!aris::legal::LoadLegalDocuments(legalRoot.string(), legalDocuments) ||
        legalDocuments.eulaText.find("唯一 EULA 内容") == std::string::npos ||
        legalDocuments.privacyText.find("唯一隐私内容") == std::string::npos) {
        std::printf("FAIL: both legal documents must be loaded and kept distinct\n");
        return 1;
    }
#ifdef _WIN32
    const auto dialogRuntime = aris::legal::RunConsentDialogRuntimeTests();
    if (!dialogRuntime.dialogCreated || !dialogRuntime.initiallyDisabled ||
        !dialogRuntime.penultimateStillDisabled ||
        !dialogRuntime.exactBottomEnabled ||
        !dialogRuntime.prematureAcceptRejected ||
        !dialogRuntime.scrollInfoFailureRejected ||
        !dialogRuntime.presentedCallbackObserved ||
        !dialogRuntime.presentedAfterVisibleInteractive) {
        std::printf(
            "FAIL: real consent dialog behavior failed "
            "(created=%d initial=%d penultimate=%d exact=%d premature=%d failClosed=%d presented=%d interactive=%d)\n",
            dialogRuntime.dialogCreated, dialogRuntime.initiallyDisabled,
            dialogRuntime.penultimateStillDisabled,
            dialogRuntime.exactBottomEnabled,
            dialogRuntime.prematureAcceptRejected,
            dialogRuntime.scrollInfoFailureRejected,
            dialogRuntime.presentedCallbackObserved,
            dialogRuntime.presentedAfterVisibleInteractive);
        return 1;
    }
#endif
    std::filesystem::remove(legalRoot / "PRIVACY_POLICY.md", ec);
    legalDocuments = {};
    if (aris::legal::LoadLegalDocuments(legalRoot.string(), legalDocuments)) {
        std::printf("FAIL: missing privacy document must reject consent\n");
        return 1;
    }
    if (!Platform::FileSystem::WriteFileText(
            (legalRoot / "PRIVACY_POLICY.md").string(), "")) {
        std::printf("FAIL: empty privacy document fixture should be writable\n");
        return 1;
    }
    legalDocuments = {};
    if (aris::legal::LoadLegalDocuments(legalRoot.string(), legalDocuments)) {
        std::printf("FAIL: empty privacy document must reject consent\n");
        return 1;
    }
    std::filesystem::remove_all(legalRoot, ec);

    const auto path = std::filesystem::temp_directory_path() / "aris-consent-manager-test.ini";
    const auto consentPath = std::filesystem::path(path.string() + ".legal-consent");
    std::filesystem::remove(path, ec);
    std::filesystem::remove(consentPath, ec);

    aris::legal::ConsentManager manager(path.string());
    if (!manager.requiresConsent(aris::legal::kCurrentEulaVersion,
                                 aris::legal::kCurrentPrivacyVersion)) {
        std::printf("FAIL: first launch requires consent\n");
        return 1;
    }
    if (!manager.recordConsent("1.0.24", "2026-08-11T12:00:00Z")) {
        std::printf("FAIL: consent record should be written\n");
        return 1;
    }
    if (manager.requiresConsent(aris::legal::kCurrentEulaVersion,
                                aris::legal::kCurrentPrivacyVersion)) {
        std::printf("FAIL: matching versions should not re-prompt\n");
        return 1;
    }
    if (!Platform::FileSystem::WriteFileText(path.string(), "theme=light\n")) {
        std::printf("FAIL: simulated normal settings rewrite should succeed\n");
        return 1;
    }
    aris::legal::ConsentManager afterNormalExit(path.string());
    if (afterNormalExit.requiresConsent(aris::legal::kCurrentEulaVersion,
                                        aris::legal::kCurrentPrivacyVersion)) {
        std::printf("FAIL: normal settings rewrite must not delete legal consent\n");
        return 1;
    }
    if (!manager.requiresConsent("2", aris::legal::kCurrentPrivacyVersion) ||
        !manager.requiresConsent(aris::legal::kCurrentEulaVersion, "2")) {
        std::printf("FAIL: changed legal versions must re-prompt\n");
        return 1;
    }
    const auto record = manager.load();
    if (record.eulaVersion != "1" || record.privacyVersion != "1" ||
        record.acceptedAtUtc != "2026-08-11T12:00:00Z" || record.appVersion != "1.0.24") {
        std::printf("FAIL: consent record fields were not persisted\n");
        return 1;
    }
    const auto preservedPath = std::filesystem::temp_directory_path() / "aris-consent-manager-preserve.ini";
    const auto preservedConsentPath =
        std::filesystem::path(preservedPath.string() + ".legal-consent");
    std::filesystem::remove(preservedPath, ec);
    std::filesystem::remove(preservedConsentPath, ec);
    const std::string preservedSettings = "theme=dark\nview=grid\n";
    Platform::FileSystem::WriteFileText(preservedPath.string(), preservedSettings);
    aris::legal::ConsentManager preserving(preservedPath.string());
    if (!preserving.recordConsent("1.0.24", "2026-08-11T12:00:00Z")) {
        std::printf("FAIL: consent update should preserve existing settings\n");
        return 1;
    }
    std::string preservedText;
    Platform::FileSystem::ReadFileText(preservedPath.string(), preservedText);
    if (preservedText != preservedSettings) {
        std::printf("FAIL: legal consent write must not modify user settings\n");
        return 1;
    }

    const auto incompletePath = std::filesystem::temp_directory_path() / "aris-consent-manager-incomplete.ini";
    const auto incompleteConsentPath =
        std::filesystem::path(incompletePath.string() + ".legal-consent");
    std::filesystem::remove(incompletePath, ec);
    std::filesystem::remove(incompleteConsentPath, ec);
    if (!Platform::FileSystem::WriteFileText(
            incompleteConsentPath.string(),
            "eulaVersion=1\nprivacyVersion=1\nacceptedAtUtc=2026-08-11T12:00:00Z\n")) {
        std::printf("FAIL: incomplete consent fixture should be writable\n");
        return 1;
    }
    aris::legal::ConsentManager incomplete(incompletePath.string());
    if (!incomplete.requiresConsent("1", "1")) {
        std::printf("FAIL: consent without appVersion must re-prompt\n");
        return 1;
    }
    std::filesystem::remove(preservedPath, ec);
    std::filesystem::remove(preservedConsentPath, ec);
    std::filesystem::remove(incompletePath, ec);
    std::filesystem::remove(incompleteConsentPath, ec);
    std::filesystem::remove(path, ec);
    std::filesystem::remove(consentPath, ec);
    std::printf("PASS: consent manager versions and persisted fields\n");
    return 0;
}
