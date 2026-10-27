#include "VoiceAssetOrganizer.h"

#include <iostream>
#include <string>

namespace {

bool Expect(bool condition, const char* message) {
    if (condition) {
        std::cout << "PASS: " << message << "\n";
        return true;
    }
    std::cerr << "FAIL: " << message << "\n";
    return false;
}

TranscriptionResult MakeCombatTranscript() {
    TranscriptionResult result;
    result.success = true;
    result.language = "zh";
    result.fullText = u8"敌人来了，快走，这里危险";
    result.segments.push_back({0, 1200, u8"敌人来了", 0.9f});
    result.segments.push_back({1300, 2600, u8"快走，这里危险", 0.9f});
    return result;
}

} // namespace

int main() {
    bool ok = true;

    TranscriptionResult combat = MakeCombatTranscript();
    VoiceAssetOrganization org = VoiceAssetOrganizer::Analyze("npc_combat_01.wav", combat, 2.8f);
    ok &= Expect(org.searchableDescription.find("Transcript:") != std::string::npos,
                 "description includes searchable transcript excerpt");
    ok &= Expect(org.keywordCsv.find("combat-dialogue") != std::string::npos,
                 "combat transcript generates combat keyword");
    ok &= Expect(org.keywordCsv.find("language:zh") != std::string::npos,
                 "Chinese transcript generates language keyword");
    ok &= Expect(!org.needsHumanReview,
                 "normal transcript does not require review");

    TranscriptionResult sparse;
    sparse.success = true;
    sparse.language = "en";
    sparse.fullText = "go";
    VoiceAssetOrganization sparseOrg = VoiceAssetOrganizer::Analyze("line.wav", sparse, 12.0f);
    ok &= Expect(sparseOrg.needsHumanReview,
                 "sparse transcript requires human review");
    ok &= Expect(sparseOrg.keywordCsv.find("review-needed") != std::string::npos,
                 "review-needed keyword is emitted");

    std::string srt = VoiceAssetOrganizer::BuildSrt(combat);
    ok &= Expect(srt.find("00:00:00,000 --> 00:00:01,200") != std::string::npos,
                 "SRT export formats segment timestamps");

    std::string csv = VoiceAssetOrganizer::BuildCsv(combat);
    ok &= Expect(csv.find("start_ms,end_ms,text") == 0,
                 "CSV export includes header");

    std::string json = VoiceAssetOrganizer::BuildJson(combat);
    ok &= Expect(json.find("\"segments\"") != std::string::npos,
                 "JSON export includes segments");

    return ok ? 0 : 1;
}
