#pragma once

#include <map>
#include <string>
#include <vector>

enum class VoiceScriptField {
    MatchKey,
    ChineseLine,
    EnglishLine,
    Character,
    Emotion,
    Note,
    Language,
    ExpectedDuration
};

struct VoiceScriptColumnMapping {
    std::map<VoiceScriptField, int> fieldToColumn;

    bool has(VoiceScriptField field) const;
    int column(VoiceScriptField field) const;
};

struct VoiceScriptRow {
    int sourceRow = 0;
    std::string matchKey;
    std::string chineseLine;
    std::string englishLine;
    std::string character;
    std::string emotion;
    std::string note;
    std::string language;
    std::string expectedDuration;
};

struct VoiceScriptImportIssue {
    int sourceRow = 0;
    std::string code;
    std::string message;
    int severity = 0; // 0=info, 1=warning, 2=blocking
};

struct VoiceScriptImportResult {
    std::vector<std::string> headers;
    std::vector<VoiceScriptRow> rows;
    std::vector<VoiceScriptImportIssue> issues;

    bool hasBlockingIssues() const;
};

enum class VoiceDeliveryMatchStatus {
    Matched,
    MissingFile,
    ExtraFile,
    DuplicateDeliveredFile
};

enum class VoiceLineCheckStatus {
    Pass,
    Review,
    Fail
};

struct VoiceLineCheckResult {
    VoiceLineCheckStatus status = VoiceLineCheckStatus::Fail;
    float similarity = 0.0f;
    std::string normalizedExpected;
    std::string normalizedActual;
    std::string message;
};

struct VoiceDeliveredFile {
    std::string path;
    std::string fileName;
    std::string matchKey;
};

struct VoiceDeliveryMatchItem {
    VoiceDeliveryMatchStatus status = VoiceDeliveryMatchStatus::MissingFile;
    std::string matchKey;
    int sourceRow = 0;
    std::string scriptText;
    std::vector<VoiceDeliveredFile> files;
};

struct VoiceDeliveryMatchReport {
    std::vector<VoiceDeliveryMatchItem> items;
    int matchedCount = 0;
    int missingFileCount = 0;
    int extraFileCount = 0;
    int duplicateDeliveredFileCount = 0;

    bool hasBlockingIssues() const;
};

struct VoiceAcceptanceReportRow {
    std::string matchKey;
    int sourceRow = 0;
    VoiceDeliveryMatchStatus matchStatus = VoiceDeliveryMatchStatus::MissingFile;
    VoiceLineCheckStatus lineStatus = VoiceLineCheckStatus::Fail;
    float lineSimilarity = 0.0f;
    std::string deliveredFile;
    std::string expectedText;
    std::string actualTranscript;
    std::string action;
};

struct VoiceAcceptanceReport {
    std::vector<VoiceAcceptanceReportRow> rows;
    int passCount = 0;
    int reviewCount = 0;
    int failCount = 0;
};

struct VoiceMetadataWritebackSuggestion {
    std::string matchKey;
    std::string description;
    std::string keywordCsv;
    bool safeToApply = false;
};

// @stage7-contract
// API contract: VoiceDeliveryAcceptance performs synchronous deterministic
// parsing, matching, comparison, and report formatting over caller-owned
// values. All returned rows/reports/strings are owned; inputs are not retained,
// and validation failures are represented by issue/status fields rather than
// filesystem or metadata mutation.
class VoiceDeliveryAcceptance {
public:
    static std::vector<std::vector<std::string>> ParseDelimitedText(const std::string& text,
                                                                    char delimiter = ',');

    static VoiceScriptImportResult ImportRows(const std::vector<std::vector<std::string>>& table,
                                              const VoiceScriptColumnMapping& mapping,
                                              bool firstRowIsHeader = true);

    static VoiceDeliveryMatchReport MatchDeliveryFiles(const std::vector<VoiceScriptRow>& scriptRows,
                                                       const std::vector<std::string>& deliveredFilePaths);

    static VoiceLineCheckResult CompareLineText(const std::string& expected,
                                                const std::string& actual);

    static VoiceAcceptanceReport BuildAcceptanceReport(
        const VoiceDeliveryMatchReport& matchReport,
        const std::map<std::string, std::string>& transcriptsByMatchKey);

    static std::string ExportAcceptanceReportCsv(const VoiceAcceptanceReport& report);

    static VoiceMetadataWritebackSuggestion BuildMetadataSuggestion(const VoiceAcceptanceReportRow& row);

    static std::string NormalizeMatchKey(const std::string& value);
    static const char* FieldLabel(VoiceScriptField field);
    static std::vector<VoiceScriptField> RequiredFields();
};
