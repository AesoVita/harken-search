#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Stable global type identities retained for existing compatibility aliases.
enum class RenameMode {
    TextReplace = 0,
    RegexReplace,
    DeleteText,
    AddPrefix,
    AddSuffix,
    DeleteStartChars,
    DeleteEndChars,
    DeleteChinese,
    DeleteEnglish,
    DeleteNumbers,
    DeletePunctuation,
    UpperToTitle,
    TraditionalToSimplified,
    RemoveExtraBrackets,
    ExtractBracketContent,
    InsertSequenceNumber,
    ToLowercase,
    ToUppercase,
    ReplaceSpaces,
    TrimSpaces,
    DeleteFromMatchedTextToEnd,
    COUNT
};

struct RenameRule {
    RenameMode mode = RenameMode::TextReplace;
    std::string searchText;
    std::string replaceText;
    std::string description;
};

struct RenamePreview {
    std::string oldName;
    std::string newName;
    std::string filePath;
    int64_t recordId;
    bool willChange;
    std::string status;
    std::string conflict;
};

namespace aura::rename {

struct PreviewInput {
    std::string fileName;
    std::string filePath;
    int64_t recordId = 0;
};

struct PreviewLabels {
    std::string noRules;
    std::string noChange;
    std::string emptyName;
    std::string duplicate;
    std::string targetExists;
    std::string ready;
    std::string localizedSuffixToken;
};

// @stage7-contract
// API contract: rename types retain their global identities and field order.
// Inputs are borrowed synchronously; returned strings and previews are owned.
// Localized values are supplied by the caller and are never retained.
std::string ApplyRule(
    const std::string& nameWithoutExtension,
    const RenameRule& rule,
    int sequenceNumber,
    const std::string& localizedSuffixToken);

std::string ApplyRulesToFileName(
    const std::string& fileName,
    const std::vector<RenameRule>& rules,
    int sequenceNumber,
    const std::string& localizedSuffixToken);

std::vector<RenamePreview> BuildPreview(
    const std::vector<PreviewInput>& inputs,
    const std::vector<RenameRule>& rules,
    const PreviewLabels& labels);

} // namespace aura::rename
