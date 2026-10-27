#include "VoiceDeliveryAcceptance.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <numeric>
#include <set>

namespace {

std::string Trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' ||
                                    value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string Cell(const std::vector<std::string>& row, int column) {
    if (column < 0 || column >= (int)row.size()) return "";
    return Trim(row[(size_t)column]);
}

std::string PreferredScriptText(const VoiceScriptRow& row) {
    if (!row.chineseLine.empty()) return row.chineseLine;
    return row.englishLine;
}

const char* MatchStatusName(VoiceDeliveryMatchStatus status) {
    switch (status) {
        case VoiceDeliveryMatchStatus::Matched: return "Matched";
        case VoiceDeliveryMatchStatus::MissingFile: return "MissingFile";
        case VoiceDeliveryMatchStatus::ExtraFile: return "ExtraFile";
        case VoiceDeliveryMatchStatus::DuplicateDeliveredFile: return "DuplicateDeliveredFile";
        default: return "Unknown";
    }
}

const char* LineStatusName(VoiceLineCheckStatus status) {
    switch (status) {
        case VoiceLineCheckStatus::Pass: return "PASS";
        case VoiceLineCheckStatus::Review: return "REVIEW";
        case VoiceLineCheckStatus::Fail: return "FAIL";
        default: return "UNKNOWN";
    }
}

std::string EscapeCsv(std::string value) {
    bool needsQuote = false;
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"') {
            escaped += "\"\"";
            needsQuote = true;
        } else {
            if (c == ',' || c == '\n' || c == '\r') needsQuote = true;
            escaped.push_back(c);
        }
    }
    return needsQuote ? "\"" + escaped + "\"" : escaped;
}

std::vector<uint32_t> DecodeUtf8(const std::string& text) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            out.push_back(c);
            ++i;
        } else if ((c >> 5) == 0x6 && i + 1 < text.size()) {
            out.push_back(((uint32_t)(c & 0x1F) << 6) | ((uint32_t)(text[i + 1] & 0x3F)));
            i += 2;
        } else if ((c >> 4) == 0xE && i + 2 < text.size()) {
            out.push_back(((uint32_t)(c & 0x0F) << 12) |
                          ((uint32_t)(text[i + 1] & 0x3F) << 6) |
                          ((uint32_t)(text[i + 2] & 0x3F)));
            i += 3;
        } else if ((c >> 3) == 0x1E && i + 3 < text.size()) {
            out.push_back(((uint32_t)(c & 0x07) << 18) |
                          ((uint32_t)(text[i + 1] & 0x3F) << 12) |
                          ((uint32_t)(text[i + 2] & 0x3F) << 6) |
                          ((uint32_t)(text[i + 3] & 0x3F)));
            i += 4;
        } else {
            ++i;
        }
    }
    return out;
}

bool IsIgnoredCodepoint(uint32_t cp) {
    if (cp <= 0x20) return true;
    if (cp < 0x80 && std::ispunct((unsigned char)cp)) return true;
    switch (cp) {
        case 0x3002: // 。
        case 0xFF0C: // ，
        case 0xFF01: // ！
        case 0xFF1F: // ？
        case 0xFF1A: // ：
        case 0xFF1B: // ；
        case 0x3001: // 、
        case 0x201C: // “
        case 0x201D: // ”
        case 0x2018: // ‘
        case 0x2019: // ’
            return true;
        default:
            return false;
    }
}

std::vector<uint32_t> NormalizeTextUnits(const std::string& text) {
    std::vector<uint32_t> units;
    for (uint32_t cp : DecodeUtf8(text)) {
        if (IsIgnoredCodepoint(cp)) continue;
        if (cp < 0x80) cp = (uint32_t)std::tolower((unsigned char)cp);
        units.push_back(cp);
    }
    return units;
}

std::string UnitsDebugString(const std::vector<uint32_t>& units) {
    std::string out;
    for (uint32_t cp : units) {
        if (cp < 0x80) out.push_back((char)cp);
        else out += "?";
    }
    return out;
}

int EditDistance(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
    std::vector<int> prev(b.size() + 1);
    std::vector<int> cur(b.size() + 1);
    std::iota(prev.begin(), prev.end(), 0);
    for (size_t i = 1; i <= a.size(); ++i) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= b.size(); ++j) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }
    return prev[b.size()];
}

void AddIssue(VoiceScriptImportResult& result,
              int sourceRow,
              const std::string& code,
              const std::string& message,
              int severity) {
    result.issues.push_back({sourceRow, code, message, severity});
}

} // namespace

bool VoiceScriptColumnMapping::has(VoiceScriptField field) const {
    return fieldToColumn.find(field) != fieldToColumn.end();
}

int VoiceScriptColumnMapping::column(VoiceScriptField field) const {
    auto it = fieldToColumn.find(field);
    return it == fieldToColumn.end() ? -1 : it->second;
}

bool VoiceScriptImportResult::hasBlockingIssues() const {
    return std::any_of(issues.begin(), issues.end(), [](const VoiceScriptImportIssue& issue) {
        return issue.severity >= 2;
    });
}

bool VoiceDeliveryMatchReport::hasBlockingIssues() const {
    return missingFileCount > 0 || duplicateDeliveredFileCount > 0;
}

std::vector<std::vector<std::string>> VoiceDeliveryAcceptance::ParseDelimitedText(const std::string& text,
                                                                                  char delimiter) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool inQuotes = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            if (inQuotes && i + 1 < text.size() && text[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delimiter && !inQuotes) {
            row.push_back(cell);
            cell.clear();
        } else if ((c == '\n' || c == '\r') && !inQuotes) {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            row.push_back(cell);
            cell.clear();
            rows.push_back(row);
            row.clear();
        } else {
            cell.push_back(c);
        }
    }

    if (!cell.empty() || !row.empty()) {
        row.push_back(cell);
        rows.push_back(row);
    }
    return rows;
}

VoiceScriptImportResult VoiceDeliveryAcceptance::ImportRows(const std::vector<std::vector<std::string>>& table,
                                                            const VoiceScriptColumnMapping& mapping,
                                                            bool firstRowIsHeader) {
    VoiceScriptImportResult result;
    if (table.empty()) {
        AddIssue(result, 0, "empty_table", "Script table is empty.", 2);
        return result;
    }

    const int firstDataRow = firstRowIsHeader ? 1 : 0;
    if (firstRowIsHeader) {
        result.headers = table.front();
    }

    for (VoiceScriptField field : RequiredFields()) {
        if (!mapping.has(field)) {
            AddIssue(result, 0, "missing_mapping",
                     std::string("Required field is not mapped: ") + FieldLabel(field), 2);
        }
    }
    if (result.hasBlockingIssues()) return result;

    std::set<std::string> seenKeys;
    for (int i = firstDataRow; i < (int)table.size(); ++i) {
        const auto& raw = table[(size_t)i];
        const int sourceRow = i + 1;
        VoiceScriptRow row;
        row.sourceRow = sourceRow;
        row.matchKey = Cell(raw, mapping.column(VoiceScriptField::MatchKey));
        row.chineseLine = Cell(raw, mapping.column(VoiceScriptField::ChineseLine));
        row.englishLine = Cell(raw, mapping.column(VoiceScriptField::EnglishLine));
        row.character = Cell(raw, mapping.column(VoiceScriptField::Character));
        row.emotion = Cell(raw, mapping.column(VoiceScriptField::Emotion));
        row.note = Cell(raw, mapping.column(VoiceScriptField::Note));
        row.language = Cell(raw, mapping.column(VoiceScriptField::Language));
        row.expectedDuration = Cell(raw, mapping.column(VoiceScriptField::ExpectedDuration));

        if (row.matchKey.empty()) {
            AddIssue(result, sourceRow, "missing_match_key", "Row has no file matching key.", 2);
        } else if (!seenKeys.insert(row.matchKey).second) {
            AddIssue(result, sourceRow, "duplicate_match_key", "Duplicate file matching key: " + row.matchKey, 2);
        }
        if (row.chineseLine.empty() && row.englishLine.empty()) {
            AddIssue(result, sourceRow, "missing_line_text", "Row has no Chinese or English script text.", 1);
        }

        result.rows.push_back(row);
    }

    return result;
}

VoiceDeliveryMatchReport VoiceDeliveryAcceptance::MatchDeliveryFiles(
    const std::vector<VoiceScriptRow>& scriptRows,
    const std::vector<std::string>& deliveredFilePaths) {
    VoiceDeliveryMatchReport report;

    std::map<std::string, std::vector<VoiceDeliveredFile>> filesByKey;
    for (const std::string& path : deliveredFilePaths) {
        std::filesystem::path fsPath(path);
        VoiceDeliveredFile file;
        file.path = path;
        file.fileName = fsPath.filename().u8string();
        file.matchKey = NormalizeMatchKey(file.fileName);
        filesByKey[file.matchKey].push_back(file);
    }

    std::set<std::string> expectedKeys;
    for (const VoiceScriptRow& row : scriptRows) {
        const std::string key = NormalizeMatchKey(row.matchKey);
        if (key.empty()) continue;
        expectedKeys.insert(key);

        VoiceDeliveryMatchItem item;
        item.matchKey = key;
        item.sourceRow = row.sourceRow;
        item.scriptText = PreferredScriptText(row);

        auto fileIt = filesByKey.find(key);
        if (fileIt == filesByKey.end() || fileIt->second.empty()) {
            item.status = VoiceDeliveryMatchStatus::MissingFile;
            ++report.missingFileCount;
        } else if (fileIt->second.size() > 1) {
            item.status = VoiceDeliveryMatchStatus::DuplicateDeliveredFile;
            item.files = fileIt->second;
            ++report.duplicateDeliveredFileCount;
        } else {
            item.status = VoiceDeliveryMatchStatus::Matched;
            item.files = fileIt->second;
            ++report.matchedCount;
        }
        report.items.push_back(item);
    }

    for (const auto& [key, files] : filesByKey) {
        if (expectedKeys.find(key) != expectedKeys.end()) continue;
        VoiceDeliveryMatchItem item;
        item.status = VoiceDeliveryMatchStatus::ExtraFile;
        item.matchKey = key;
        item.files = files;
        report.extraFileCount += (int)files.size();
        report.items.push_back(item);
    }

    return report;
}

std::string VoiceDeliveryAcceptance::NormalizeMatchKey(const std::string& value) {
    std::string key = Trim(value);
    std::replace(key.begin(), key.end(), '\\', '/');
    size_t slash = key.find_last_of('/');
    if (slash != std::string::npos) key = key.substr(slash + 1);
    size_t dot = key.find_last_of('.');
    if (dot != std::string::npos) key = key.substr(0, dot);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return Trim(key);
}

VoiceLineCheckResult VoiceDeliveryAcceptance::CompareLineText(const std::string& expected,
                                                              const std::string& actual) {
    VoiceLineCheckResult result;
    const auto expectedUnits = NormalizeTextUnits(expected);
    const auto actualUnits = NormalizeTextUnits(actual);
    result.normalizedExpected = UnitsDebugString(expectedUnits);
    result.normalizedActual = UnitsDebugString(actualUnits);

    if (expectedUnits.empty() || actualUnits.empty()) {
        result.status = VoiceLineCheckStatus::Fail;
        result.similarity = 0.0f;
        result.message = "Expected or actual line is empty after normalization.";
        return result;
    }

    const int distance = EditDistance(expectedUnits, actualUnits);
    const int maxLen = (int)std::max(expectedUnits.size(), actualUnits.size());
    result.similarity = maxLen == 0 ? 0.0f : 1.0f - ((float)distance / (float)maxLen);

    if (result.similarity >= 0.85f) {
        result.status = VoiceLineCheckStatus::Pass;
        result.message = "Transcript matches the script line.";
    } else if (result.similarity >= 0.65f) {
        result.status = VoiceLineCheckStatus::Review;
        result.message = "Transcript is close but needs human review.";
    } else {
        result.status = VoiceLineCheckStatus::Fail;
        result.message = "Transcript differs from the script line.";
    }
    return result;
}

VoiceAcceptanceReport VoiceDeliveryAcceptance::BuildAcceptanceReport(
    const VoiceDeliveryMatchReport& matchReport,
    const std::map<std::string, std::string>& transcriptsByMatchKey) {
    VoiceAcceptanceReport report;
    for (const VoiceDeliveryMatchItem& item : matchReport.items) {
        VoiceAcceptanceReportRow row;
        row.matchKey = item.matchKey;
        row.sourceRow = item.sourceRow;
        row.matchStatus = item.status;
        row.expectedText = item.scriptText;
        if (!item.files.empty()) row.deliveredFile = item.files.front().path;

        auto transcriptIt = transcriptsByMatchKey.find(item.matchKey);
        if (transcriptIt != transcriptsByMatchKey.end()) {
            row.actualTranscript = transcriptIt->second;
        }

        if (item.status != VoiceDeliveryMatchStatus::Matched) {
            row.lineStatus = VoiceLineCheckStatus::Fail;
            row.action = item.status == VoiceDeliveryMatchStatus::ExtraFile
                ? "Return or map extra file"
                : "Fix delivery file set";
        } else if (row.actualTranscript.empty()) {
            row.lineStatus = VoiceLineCheckStatus::Review;
            row.action = "Run ASR before content acceptance";
        } else {
            VoiceLineCheckResult check = CompareLineText(row.expectedText, row.actualTranscript);
            row.lineStatus = check.status;
            row.lineSimilarity = check.similarity;
            row.action = check.status == VoiceLineCheckStatus::Pass
                ? "Accept"
                : (check.status == VoiceLineCheckStatus::Review ? "Human review" : "Return for re-record");
        }

        if (row.lineStatus == VoiceLineCheckStatus::Pass) ++report.passCount;
        else if (row.lineStatus == VoiceLineCheckStatus::Review) ++report.reviewCount;
        else ++report.failCount;
        report.rows.push_back(row);
    }
    return report;
}

std::string VoiceDeliveryAcceptance::ExportAcceptanceReportCsv(const VoiceAcceptanceReport& report) {
    std::string csv = "match_key,source_row,match_status,line_status,similarity,delivered_file,expected_text,actual_transcript,action\n";
    for (const VoiceAcceptanceReportRow& row : report.rows) {
        csv += EscapeCsv(row.matchKey) + ",";
        csv += std::to_string(row.sourceRow) + ",";
        csv += MatchStatusName(row.matchStatus);
        csv += ",";
        csv += LineStatusName(row.lineStatus);
        csv += ",";
        csv += std::to_string(row.lineSimilarity);
        csv += ",";
        csv += EscapeCsv(row.deliveredFile) + ",";
        csv += EscapeCsv(row.expectedText) + ",";
        csv += EscapeCsv(row.actualTranscript) + ",";
        csv += EscapeCsv(row.action) + "\n";
    }
    return csv;
}

VoiceMetadataWritebackSuggestion VoiceDeliveryAcceptance::BuildMetadataSuggestion(
    const VoiceAcceptanceReportRow& row) {
    VoiceMetadataWritebackSuggestion suggestion;
    suggestion.matchKey = row.matchKey;
    suggestion.safeToApply =
        row.matchStatus == VoiceDeliveryMatchStatus::Matched &&
        row.lineStatus == VoiceLineCheckStatus::Pass &&
        !row.expectedText.empty();

    const std::string acceptedText = !row.expectedText.empty() ? row.expectedText : row.actualTranscript;
    suggestion.description = "Voice delivery accepted";
    if (!acceptedText.empty()) {
        suggestion.description += ". Line: " + acceptedText;
    }
    if (!row.deliveredFile.empty()) {
        suggestion.description += ". File: " + row.deliveredFile;
    }

    std::vector<std::string> keywords = {"voice", "delivery-review", "scripted-line"};
    if (row.matchStatus == VoiceDeliveryMatchStatus::Matched) keywords.push_back("matched");
    if (row.lineStatus == VoiceLineCheckStatus::Pass) keywords.push_back("line-pass");
    else if (row.lineStatus == VoiceLineCheckStatus::Review) keywords.push_back("line-review");
    else keywords.push_back("line-fail");
    if (!suggestion.safeToApply) keywords.push_back("review-needed");

    for (size_t i = 0; i < keywords.size(); ++i) {
        if (i > 0) suggestion.keywordCsv += ", ";
        suggestion.keywordCsv += keywords[i];
    }
    return suggestion;
}

const char* VoiceDeliveryAcceptance::FieldLabel(VoiceScriptField field) {
    switch (field) {
        case VoiceScriptField::MatchKey: return "File match key";
        case VoiceScriptField::ChineseLine: return "Chinese line";
        case VoiceScriptField::EnglishLine: return "English line";
        case VoiceScriptField::Character: return "Character";
        case VoiceScriptField::Emotion: return "Emotion";
        case VoiceScriptField::Note: return "Note";
        case VoiceScriptField::Language: return "Language";
        case VoiceScriptField::ExpectedDuration: return "Expected duration";
        default: return "Unknown";
    }
}

std::vector<VoiceScriptField> VoiceDeliveryAcceptance::RequiredFields() {
    return {
        VoiceScriptField::MatchKey,
    };
}
