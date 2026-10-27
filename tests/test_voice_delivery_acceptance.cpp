#include "VoiceDeliveryAcceptance.h"

#include <iostream>

namespace {

bool Expect(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << "\n";
        return true;
    }
    std::cerr << "FAIL: " << message << "\n";
    return false;
}

} // namespace

int main() {
    bool ok = true;

    const std::string csv =
        "FileId,Chinese,Character,Emotion\n"
        "VO_001,\"快走，这里要塌了\",NPC_A,urgent\n"
        "VO_002,敌人来了,NPC_B,combat\n";

    auto table = VoiceDeliveryAcceptance::ParseDelimitedText(csv);
    ok &= Expect(table.size() == 3, "CSV parser keeps header and two rows");
    ok &= Expect(table[1][1] == u8"快走，这里要塌了", "CSV parser handles quoted Chinese text");

    VoiceScriptColumnMapping mapping;
    mapping.fieldToColumn[VoiceScriptField::MatchKey] = 0;
    mapping.fieldToColumn[VoiceScriptField::ChineseLine] = 1;
    mapping.fieldToColumn[VoiceScriptField::Character] = 2;
    mapping.fieldToColumn[VoiceScriptField::Emotion] = 3;

    VoiceScriptImportResult imported = VoiceDeliveryAcceptance::ImportRows(table, mapping, true);
    ok &= Expect(!imported.hasBlockingIssues(), "valid mapped table imports without blockers");
    ok &= Expect(imported.rows.size() == 2, "valid mapped table imports two rows");
    ok &= Expect(imported.rows[0].matchKey == "VO_001", "match key maps from selected column");
    ok &= Expect(imported.rows[0].chineseLine == u8"快走，这里要塌了", "Chinese line maps from selected column");

    VoiceScriptColumnMapping missingMapping;
    VoiceScriptImportResult missing = VoiceDeliveryAcceptance::ImportRows(table, missingMapping, true);
    ok &= Expect(missing.hasBlockingIssues(), "missing required match key mapping blocks import");

    const std::string duplicateCsv =
        "FileId,Chinese\n"
        "VO_001,第一句\n"
        "VO_001,第二句\n";
    auto duplicateTable = VoiceDeliveryAcceptance::ParseDelimitedText(duplicateCsv);
    VoiceScriptColumnMapping duplicateMapping;
    duplicateMapping.fieldToColumn[VoiceScriptField::MatchKey] = 0;
    duplicateMapping.fieldToColumn[VoiceScriptField::ChineseLine] = 1;
    VoiceScriptImportResult duplicate = VoiceDeliveryAcceptance::ImportRows(duplicateTable, duplicateMapping, true);
    ok &= Expect(duplicate.hasBlockingIssues(), "duplicate match keys block acceptance import");

    std::vector<VoiceScriptRow> scriptRows;
    VoiceScriptRow row1;
    row1.sourceRow = 2;
    row1.matchKey = "VO_001";
    row1.chineseLine = "line one";
    scriptRows.push_back(row1);
    VoiceScriptRow row2;
    row2.sourceRow = 3;
    row2.matchKey = "VO_002.wav";
    row2.chineseLine = "line two";
    scriptRows.push_back(row2);
    VoiceScriptRow row3;
    row3.sourceRow = 4;
    row3.matchKey = "VO_003";
    row3.chineseLine = "line three";
    scriptRows.push_back(row3);

    std::vector<std::string> deliveredFiles = {
        "D:/delivery/VO_001.wav",
        "D:/delivery/VO_002.wav",
        "D:/delivery/VO_002_take2.wav",
        "D:/delivery/extra_line.wav",
    };
    VoiceDeliveryMatchReport match = VoiceDeliveryAcceptance::MatchDeliveryFiles(scriptRows, deliveredFiles);
    ok &= Expect(match.matchedCount == 2, "delivery matching accepts extension and path differences");
    ok &= Expect(match.missingFileCount == 1, "delivery matching reports missing script files");
    ok &= Expect(match.extraFileCount == 2, "delivery matching reports extra delivered files");
    ok &= Expect(match.hasBlockingIssues(), "missing files block delivery acceptance");

    std::vector<std::string> duplicatedFiles = {
        "VO_001.wav",
        "VO_001.mp3",
        "VO_002.wav",
        "VO_003.wav",
    };
    VoiceDeliveryMatchReport duplicateFiles =
        VoiceDeliveryAcceptance::MatchDeliveryFiles(scriptRows, duplicatedFiles);
    ok &= Expect(duplicateFiles.duplicateDeliveredFileCount == 1,
                 "delivery matching reports duplicate files for the same script key");

    VoiceLineCheckResult pass =
        VoiceDeliveryAcceptance::CompareLineText("Reload now!", "reload now");
    ok &= Expect(pass.status == VoiceLineCheckStatus::Pass,
                 "line check passes punctuation and case differences");

    VoiceLineCheckResult review =
        VoiceDeliveryAcceptance::CompareLineText("enemy approaching north gate",
                                                 "enemy approaching gate");
    ok &= Expect(review.status == VoiceLineCheckStatus::Review,
                 "line check marks partial omissions for review");

    VoiceLineCheckResult fail =
        VoiceDeliveryAcceptance::CompareLineText("open the main gate",
                                                 "the weather is quiet");
    ok &= Expect(fail.status == VoiceLineCheckStatus::Fail,
                 "line check fails unrelated transcript text");

    std::map<std::string, std::string> transcripts;
    transcripts["vo_001"] = "line one";
    transcripts["vo_002"] = "line two";
    VoiceAcceptanceReport acceptance =
        VoiceDeliveryAcceptance::BuildAcceptanceReport(match, transcripts);
    ok &= Expect(acceptance.rows.size() == match.items.size(),
                 "acceptance report preserves every match item");
    ok &= Expect(acceptance.passCount == 2,
                 "acceptance report counts passing matched transcripts");
    ok &= Expect(acceptance.failCount >= 1,
                 "acceptance report counts missing and extra files as failures");

    std::string reportCsv = VoiceDeliveryAcceptance::ExportAcceptanceReportCsv(acceptance);
    ok &= Expect(reportCsv.find("match_key,source_row,match_status,line_status") == 0,
                 "acceptance report CSV includes stable header");
    ok &= Expect(reportCsv.find("Return for re-record") == std::string::npos,
                 "acceptance report does not request rerecord for passing sample rows");
    ok &= Expect(reportCsv.find("Fix delivery file set") != std::string::npos,
                 "acceptance report CSV includes file-set fix action");

    VoiceMetadataWritebackSuggestion safeSuggestion =
        VoiceDeliveryAcceptance::BuildMetadataSuggestion(acceptance.rows[0]);
    ok &= Expect(safeSuggestion.safeToApply,
                 "metadata suggestion is safe only for matched passing rows");
    ok &= Expect(safeSuggestion.keywordCsv.find("line-pass") != std::string::npos,
                 "metadata suggestion includes acceptance keyword");

    VoiceMetadataWritebackSuggestion unsafeSuggestion;
    for (const auto& row : acceptance.rows) {
        if (row.matchStatus != VoiceDeliveryMatchStatus::Matched) {
            unsafeSuggestion = VoiceDeliveryAcceptance::BuildMetadataSuggestion(row);
            break;
        }
    }
    ok &= Expect(!unsafeSuggestion.safeToApply,
                 "metadata suggestion blocks missing or extra file rows");
    ok &= Expect(unsafeSuggestion.keywordCsv.find("review-needed") != std::string::npos,
                 "unsafe metadata suggestion is tagged for review");

    return ok ? 0 : 1;
}
