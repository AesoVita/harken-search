#include "Utils.h"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

} // namespace

int main() {
    bool ok = true;
    ok &= Check(Utf8ToWide("").empty(), "empty UTF-8 remains empty");
    ok &= Check(WideToUtf8(L"").empty(), "empty wide text remains empty");

    std::vector<std::string> values = {
        "ascii file name.wav",
        u8"中文 音效.wav",
        u8"emoji-🔊 path.wav",
    };
    std::string longValue;
    for (int index = 0; index < 2048; ++index) {
        longValue += u8"片段 🔊 ";
    }
    values.push_back(longValue);
    for (const std::string& value : values) {
        ok &= Check(WideToUtf8(Utf8ToWide(value)) == value,
                    "valid UTF-8 real-TU round trip changed");
    }

    const std::string malformed("\xC3\x28", 2);
    const std::string mojibakeChinese(
        "\xE6\xB6\x93\xEE\x85\x9F\xE6\x9E\x83", 9);
    const std::string utf8Chinese("\xE4\xB8\xAD\xE6\x96\x87", 6);
    const std::string embeddedNul("a\0b", 3);
    const std::wstring embeddedWide(L"a\0b", 3);
#ifdef _WIN32
    const std::string replacementAndParen("\xEF\xBF\xBD\x28", 4);
    ok &= Check(WideToUtf8(Utf8ToWide(malformed)) == replacementAndParen,
                "Windows malformed UTF-8 replacement changed");
    ok &= Check(Utf8ToWide(embeddedNul) == L"a",
                "Windows UTF-8 embedded NUL no longer opens the prefix");
    ok &= Check(WideToUtf8(embeddedWide) == "a",
                "Windows wide embedded NUL no longer opens the prefix");

    const std::wstring loneSurrogate(1, static_cast<wchar_t>(0xD800));
    const std::string replacement("\xEF\xBF\xBD", 3);
    ok &= Check(WideToUtf8(loneSurrogate) == replacement,
                "Windows malformed wide replacement changed");
    const UINT systemAnsiCodePage = GetACP();
    if (systemAnsiCodePage == 936) {
        ok &= Check(RepairUtf8MojibakeFromAnsiIfNeeded(mojibakeChinese) == utf8Chinese,
                    "Windows CP936 mojibake repair changed");
    } else if (systemAnsiCodePage == 1252) {
        const std::string westernMojibake("\xC3\x83\xC2\xA9", 4);
        const std::string westernUtf8("\xC3\xA9", 2);
        ok &= Check(Utf8ToWide(westernMojibake) == L"\u00C3\u00A9" &&
                        Utf8ToWide(westernUtf8) == L"\u00E9",
                    "Windows CP1252 mojibake fixture bytes changed");
        ok &= Check(RepairUtf8MojibakeFromAnsiIfNeeded(westernMojibake) ==
                        westernUtf8,
                    "Windows CP1252 mojibake repair changed");
    }
    ok &= Check(GetPinyinInitials("ABC123" + utf8Chinese) == "abc123zw",
                "Windows CP936 pinyin initials changed");
#else
    ok &= Check(WideToUtf8(Utf8ToWide(malformed)) == malformed,
                "non-Windows byte-preserving malformed behavior changed");
    ok &= Check(WideToUtf8(Utf8ToWide(embeddedNul)) == embeddedNul,
                "non-Windows UTF-8 embedded NUL behavior changed");
    ok &= Check(WideToUtf8(embeddedWide) == embeddedNul,
                "non-Windows wide embedded NUL behavior changed");
    ok &= Check(RepairUtf8MojibakeFromAnsiIfNeeded(mojibakeChinese) == mojibakeChinese,
                "non-Windows ANSI repair behavior changed");
    ok &= Check(GetPinyinInitials("ABC123" + utf8Chinese) == "abc123",
                "non-Windows pinyin behavior changed");
#endif
    ok &= Check(RepairUtf8MojibakeFromAnsiIfNeeded(utf8Chinese) == utf8Chinese,
                "valid UTF-8 was incorrectly repaired");
    ok &= Check(RepairUtf8MojibakeFromAnsiIfNeeded(malformed) == malformed,
                "invalid input fallback changed");

    if (!ok) return 1;
    std::cout << "PASS: Utils real-TU text encoding characterization\n";
    return 0;
}
