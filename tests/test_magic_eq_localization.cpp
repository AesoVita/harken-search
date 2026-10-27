#include <cstdio>
#include <cstring>
#include "../src/Localization.h"

namespace {
int failures = 0;
void expect(const char* actual, const char* expected, const char* description) {
    if (actual && std::strcmp(actual, expected) == 0) return;
    ++failures;
    std::printf("FAIL: %s (actual=%s)\n", description, actual ? actual : "<null>");
}
}

int main() {
    const AppLanguage languages[] = {AppLanguage::Chinese, AppLanguage::English, AppLanguage::Japanese};
    const char* bypass[] = {u8"\u65c1\u901a", "BYPASS", u8"\u30d0\u30a4\u30d1\u30b9"};
    const char* pass[] = {u8"\u76f4\u901a", "PASS", u8"\u901a\u904e"};
    const char* notch[] = {u8"\u6d88\u9664", "NOTCH", u8"\u30ce\u30c3\u30c1"};
    const char* bypassCompact[] = {u8"\u65c1\u901a", "OFF", u8"\u30aa\u30d5"};
    const char* passCompact[] = {u8"\u76f4\u901a", "PASS", u8"\u901a\u904e"};
    const char* notchCompact[] = {u8"\u6d88\u9664", "CUT", u8"\u6d88\u53bb"};
    const char* soft[] = {u8"柔和", "SOFT", u8"ソフト"};
    const char* steep[] = {u8"陡峭", "STEEP", u8"スティープ"};
    const char* verySteep[] = {u8"极陡", "VERY STEEP", u8"極スティープ"};
    const char* nominal[] = {u8"标称边缘斜率", "Nominal edge slope", u8"公称エッジ傾斜"};
    const char* limited[] = {u8"受限", "LIMIT", u8"制限"};
    const char* brickwall[] = {u8"砖墙近似", "Near brickwall", u8"ブリックウォール近似"};
    const char* bandDetail[] = {
        u8"中心 %.0f Hz；-3 dB 频段 %.0f–%.0f Hz；Q %.2f",
        u8"Center %.0f Hz; -3 dB band %.0f–%.0f Hz; Q %.2f",
        u8"中心 %.0f Hz、-3 dB 帯域 %.0f–%.0f Hz、Q %.2f"
    };
    const char* tailReason[] = {
        u8"此频段的拖尾预计超过 0.2 秒。请加宽频段或选择较缓档位。",
        "Estimated tail exceeds 0.2 seconds. Widen the band or choose a gentler grade.",
        u8"この帯域の残響は 0.2 秒を超える見込みです。帯域を広げるか、緩やかな段階を選んでください。"
    };
    const char* frequencyReason[] = {
        u8"此频段超出当前采样率的可处理范围。",
        "This band is outside the supported range at the current sample rate.",
        u8"この帯域は現在のサンプルレートで処理できる範囲外です。"
    };
    const char* unloadedGradeHint[] = {
        u8"尚未加载音频。现在可以选择斜率档位；加载音频后会检查是否可用。",
        "No audio loaded. You can choose a slope now; availability will be checked after loading audio.",
        u8"音声は未読み込みです。傾斜の段階は今選べます。音声の読み込み後に利用可否を確認します。"
    };
    const char* notchExplanation[] = {
        u8"消除模式的标称边缘斜率是本产品的档位定义，并非各频点恒定的局部斜率。",
        "The Notch nominal edge slope names this product's grade; local slope varies with frequency.",
        u8"ノッチの公称エッジ傾斜は本製品の段階名です。局所傾斜は周波数によって変わります。"
    };
    for (int index = 0; index < 3; ++index) {
        g_currentLanguage = languages[index];
        expect(GetMagicEQModeLabel(false, 0), bypass[index], "bypass label is localized");
        expect(GetMagicEQModeLabel(true, 0), pass[index], "Pass label is localized");
        expect(GetMagicEQModeLabel(true, 1), notch[index], "Notch label is localized");
        expect(L(MagicEQ_ModeBypassCompact), bypassCompact[index], "compact bypass stays in one language");
        expect(L(MagicEQ_ModePassCompact), passCompact[index], "compact Pass stays in one language");
        expect(L(MagicEQ_ModeNotchCompact), notchCompact[index], "compact Notch stays in one language");
        expect(L(MagicEQ_SteepnessSoft), soft[index], "soft slope label is localized");
        expect(L(MagicEQ_SteepnessSteep), steep[index], "steep slope label is localized");
        expect(L(MagicEQ_SteepnessVerySteep), verySteep[index], "very steep slope label is localized");
        expect(L(MagicEQ_NominalEdgeSlope), nominal[index], "nominal slope title follows current language");
        expect(L(MagicEQ_LimitedPrefix), limited[index], "limited-drag bubble follows current language");
        const char* invalid[] = {
            u8"参数无效，当前效果已旁通。请重新设置频段。",
            "Invalid settings; Magic EQ is bypassed. Set the band again.",
            u8"設定が無効なため Magic EQ をバイパスしました。帯域を設定し直してください。"};
        expect(L(MagicEQ_InvalidParameters), invalid[index], "invalid input bypass reason follows current language");
        expect(L(MagicEQ_BrickwallApprox), brickwall[index], "near-wall title follows current language");
        expect(L(MagicEQ_BandDetail), bandDetail[index], "band and Q detail follows current language");
        expect(L(MagicEQ_TailTooLong), tailReason[index], "tail-budget reason follows current language");
        expect(L(MagicEQ_FrequencyUnavailable), frequencyReason[index], "frequency-limit reason follows current language");
        expect(L(MagicEQ_UnloadedGradeHint), unloadedGradeHint[index], "unloaded grade hint follows current language");
        expect(L(MagicEQ_NotchNominalExplanation), notchExplanation[index], "Notch grade explanation follows current language");
    }
    expect(GetMagicEQFrequencyLabel(500.0f, 2000.0f), "500–2.0k Hz", "frequency range uses readable UTF-8 separator");
    std::printf("Magic EQ localization: %s\n", failures == 0 ? "passed" : "failed");
    return failures == 0 ? 0 : 1;
}
