#include "SearchTextIndex.h"

#include <iostream>
#include <string>

namespace {

bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

bool Expect(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << "\n";
    return condition;
}

} // namespace

int main() {
    bool ok = true;

    const std::string ftsText = HarkenSearch::Search::BuildSearchDataForFts(
        u8"大笑 倒下 恐怖钟声 玻璃破碎 whoosh");

    ok &= Expect(Contains(ftsText, u8"大 笑"), "CJK text is separated for FTS");
    ok &= Expect(Contains(ftsText, "daxiao"), "FTS text contains full pinyin for 大笑");
    ok &= Expect(Contains(ftsText, "dx"), "FTS text contains initials for 大笑");
    ok &= Expect(Contains(ftsText, "daoxia"), "FTS text contains full pinyin for 倒下");
    ok &= Expect(Contains(ftsText, "kongbuzhongsheng"), "FTS text contains full pinyin for 恐怖钟声");
    ok &= Expect(Contains(ftsText, "kbzs"), "FTS text contains initials for 恐怖钟声");
    ok &= Expect(Contains(ftsText, "boliposui"), "FTS text contains full pinyin for 玻璃破碎");
    ok &= Expect(Contains(ftsText, "whoosh"), "FTS text preserves ASCII aliases");

    const std::string asciiAliasText = HarkenSearch::Search::BuildSearchDataForFts("SFX-01.wav");
    ok &= Expect(Contains(asciiAliasText, "sfx01wav"), "FTS text contains compact ASCII alias across separators");

#ifdef _WIN32
    const std::string fallbackInitialText = HarkenSearch::Search::BuildSearchDataForFts(u8"怪物.wav");
    ok &= Expect(Contains(fallbackInitialText, "gw"), "FTS text contains Windows fallback initials for unknown CJK");
#endif

    const std::string monsterText = HarkenSearch::Search::BuildSearchDataForFts(u8"怪物.wav");
    ok &= Expect(Contains(monsterText, "guaiwu"), "FTS text contains full pinyin for 怪物");
    ok &= Expect(Contains(monsterText, "gw"), "FTS text contains initials for 怪物");

    const auto daxiaoSpan = HarkenSearch::Search::FindPinyinSpanUtf8(u8"大笑.wav", "daxiao");
    ok &= Expect(daxiaoSpan.found, "daxiao finds a pinyin span");
    ok &= Expect(daxiaoSpan.text == u8"大笑", "daxiao span maps to 大笑");

    const auto daxiaoInitialSpan = HarkenSearch::Search::FindPinyinSpanUtf8(u8"[喜剧]大笑_人群.wav", "dx");
    ok &= Expect(daxiaoInitialSpan.found, "dx finds an initials span inside a mixed label");
    ok &= Expect(daxiaoInitialSpan.text == u8"大笑", "dx span maps to 大笑 inside a mixed label");

    const auto kbzsSpan = HarkenSearch::Search::FindPinyinSpanUtf8(u8"恐怖钟声.wav", "kbzs");
    ok &= Expect(kbzsSpan.found, "kbzs finds an initials span");
    ok &= Expect(kbzsSpan.text == u8"恐怖钟声", "kbzs span maps to 恐怖钟声");

    const auto guaiwuSpan = HarkenSearch::Search::FindPinyinSpanUtf8(u8"怪物.wav", "guaiwu");
    ok &= Expect(guaiwuSpan.found, "guaiwu finds a pinyin span");
    ok &= Expect(guaiwuSpan.text == u8"怪物", "guaiwu span maps to 怪物");

    const auto missingSpan = HarkenSearch::Search::FindPinyinSpanUtf8(u8"风声.wav", "dx");
    ok &= Expect(!missingSpan.found, "dx does not match unrelated Chinese text");

    struct RecallCase {
        const char* text;
        const char* fullPinyin;
        const char* initials;
        const char* label;
    };
    const RecallCase cases[] = {
        {u8"车", "che", "c", "车"},
        {u8"车门", "chemen", "cm", "车门"},
        {u8"汽车", "qiche", "qc", "汽车"},
        {u8"车辆", "cheliang", "cl", "车辆"},
        {u8"卡车", "kache", "kc", "卡车"},
        {u8"火车", "huoche", "hc", "火车"},
        {u8"引擎", "yinqing", "yq", "引擎"},
        {u8"轰轰", "honghong", "hh", "轰轰"},
        {u8"鑫", "xin", "x", "鑫"},
        {u8"金属刮擦", "jinshuguaca", "jsgc", "金属刮擦"},
        {u8"枪械声", "qiangxiesheng", "qxs", "枪械声"},
        {u8"大笑", "daxiao", "dx", "大笑"},
        {u8"重复", "zhongfu", "zf", "重复"},
    };
    for (const RecallCase& item : cases) {
        const std::string indexed = HarkenSearch::Search::BuildSearchDataForFts(item.text);
        const std::string fullLabel = std::string("full pinyin for ") + item.label;
        const std::string initialLabel = std::string("initials for ") + item.label;
        ok &= Expect(Contains(indexed, item.fullPinyin), fullLabel.c_str());
        ok &= Expect(Contains(indexed, item.initials), initialLabel.c_str());
    }

    // The old 82-character switch is not the coverage limit. These CJK basic-block
    // characters were absent from that list and must still produce a syllable.
    const uint32_t outsideOldList[] = {
        U'汽', U'辆', U'卡', U'引', U'擎', U'轰', U'鑫', U'库', U'索', U'输'
    };
    int covered = 0;
    for (uint32_t cp : outsideOldList) {
        if (HarkenSearch::Search::Detail::PinyinFor(cp) != nullptr) ++covered;
    }
    ok &= Expect(covered == static_cast<int>(sizeof(outsideOldList) / sizeof(outsideOldList[0])),
                 "CJK basic block outside the old 82-character list still has pinyin");
    ok &= Expect(HarkenSearch::Search::SearchDataRecomputeStillPending(0, 2048),
                 "the first 2048 rows do not finish an old library");
    ok &= Expect(!HarkenSearch::Search::SearchDataRecomputeStillPending(2048, 2048),
                 "a later pass that finds no rows finishes the library");
    ok &= Expect(HarkenSearch::Search::ClassifySearchDataRecompute(0, 2048, false)
                     == HarkenSearch::Search::SearchDataRecomputeStep::Failed,
                 "a failed pinyin write stops instead of retrying the same rows");

    // 火车 is huoche. A leading "che" query is not this word; FTS prefix search is separate.
    ok &= Expect(HarkenSearch::Search::FindPinyinSpanUtf8(u8"汽车", "che").found, "che matches 汽车");
    ok &= Expect(HarkenSearch::Search::FindPinyinSpanUtf8(u8"车辆", "che").found, "che matches 车辆");
    ok &= Expect(HarkenSearch::Search::FindPinyinSpanUtf8(u8"卡车", "che").found, "che matches 卡车");
    ok &= Expect(HarkenSearch::Search::FindPinyinSpanUtf8(u8"车", "che").found, "che matches 车");
    ok &= Expect(HarkenSearch::Search::FindPinyinSpanUtf8(u8"车门", "che").found, "che matches 车门");

    return ok ? 0 : 1;
}
