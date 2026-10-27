#pragma once

#include <string>

namespace aris::legal {

inline constexpr const char* kCurrentEulaVersion = "1";
inline constexpr const char* kCurrentPrivacyVersion = "1";

struct LegalDocuments {
    std::string eulaText;
    std::string privacyText;
};

// Loads both packaged legal documents without displaying UI or changing state.
// A missing or unreadable document returns false so startup can fail closed.
bool LoadLegalDocuments(const std::string& legalDocumentRootUtf8,
                        LegalDocuments& documents);

#ifdef _WIN32
struct ConsentDialogRuntimeTestResult {
    bool dialogCreated = false;
    bool initiallyDisabled = false;
    bool penultimateStillDisabled = false;
    bool exactBottomEnabled = false;
    bool prematureAcceptRejected = false;
    bool scrollInfoFailureRejected = false;
    bool presentedCallbackObserved = false;
    bool presentedAfterVisibleInteractive = false;
};

// Exercises the real Win32 dialog and edit-control scroll bar. The automated
// dialog closes itself; no user input is required.
ConsentDialogRuntimeTestResult RunConsentDialogRuntimeTests();
#endif

enum class ConsentResult {
    Accepted,
    Rejected,
    TechnicalFailure,
};

using ConsentUiPresentedCallback = void (*)();

struct ConsentRecord {
    std::string eulaVersion;
    std::string privacyVersion;
    std::string acceptedAtUtc;
    std::string appVersion;
};

class ConsentManager {
public:
    explicit ConsentManager(std::string settingsPath);

    ConsentRecord load() const;
    bool requiresConsent(const std::string& eulaVersion,
                         const std::string& privacyVersion) const;
    bool recordConsent(const std::string& appVersion,
                       const std::string& acceptedAtUtc,
                       const std::string& eulaVersion = kCurrentEulaVersion,
                       const std::string& privacyVersion = kCurrentPrivacyVersion) const;
    bool recordCurrentConsent(const std::string& appVersion,
                              const std::string& eulaVersion = kCurrentEulaVersion,
                              const std::string& privacyVersion = kCurrentPrivacyVersion) const;

private:
    std::string settingsPath_;
};

// Reads the two packaged legal documents and presents modal, scrollable
// document readers before recording consent. The owner is an opaque HWND so
// this header remains usable by the standalone persistence test.
ConsentResult EnsureRequiredConsent(void* ownerWindow,
                                    const std::string& settingsPath,
                                    const std::string& appVersion,
                                    const std::string& legalDocumentRootUtf8,
                                    ConsentUiPresentedCallback onFirstConsentUiPresented = nullptr);

}  // namespace aris::legal
