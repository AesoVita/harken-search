#include "platform/text_encoding.h"

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

int main() {
    const std::vector<std::string> values = {
        "",
        "ascii-file-name.wav",
        u8"中文音效.wav",
        u8"emoji-🔊.wav",
    };

    for (const std::string& value : values) {
        const std::wstring wide = Platform::TextEncoding::Utf8ToWideStrict(value);
        const std::string roundTrip = Platform::TextEncoding::WideToUtf8Strict(wide);
        if (roundTrip != value) {
            std::cerr << "FAIL: strict text encoding round trip mismatch\n";
            return 1;
        }
    }

    const std::vector<std::string> permissiveValues = {
        "",
        "folder with spaces/file.wav",
        u8"中文/音效.wav",
        u8"emoji-🔊/",
        u8"C:\\音效\\",
    };
    for (const std::string& value : permissiveValues) {
        const std::wstring wide = Platform::TextEncoding::Utf8ToWidePermissive(value);
        const std::string roundTrip = Platform::TextEncoding::WideToUtf8Permissive(wide);
        if (roundTrip != value) {
            std::cerr << "FAIL: permissive text encoding round trip mismatch\n";
            return 1;
        }
    }

    const std::string malformedUtf8("\xC3\x28", 2);
    const std::string replacementAndParen("\xEF\xBF\xBD\x28", 4);
    const std::wstring malformedWide =
        Platform::TextEncoding::Utf8ToWidePermissive(malformedUtf8);
    if (Platform::TextEncoding::WideToUtf8Permissive(malformedWide) != replacementAndParen) {
        std::cerr << "FAIL: malformed UTF-8 was not replaced with U+FFFD\n";
        return 1;
    }

    const std::wstring loneSurrogate(1, static_cast<wchar_t>(0xD800));
    const std::string replacement("\xEF\xBF\xBD", 3);
    if (Platform::TextEncoding::WideToUtf8Permissive(loneSurrogate) != replacement) {
        std::cerr << "FAIL: malformed wide input was not replaced with U+FFFD\n";
        return 1;
    }

    if (!Platform::TextEncoding::Utf8ToWidePermissive(std::string("a\0b", 3)).empty() ||
        !Platform::TextEncoding::WideToUtf8Permissive(std::wstring(L"a\0b", 3)).empty()) {
        std::cerr << "FAIL: embedded NUL was accepted\n";
        return 1;
    }

    const std::string traditionalChinese("\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87", 12);
    const std::string simplifiedChinese("\xE7\xB9\x81\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87", 12);
#ifdef _WIN32
    if (Platform::TextEncoding::ConvertTraditionalChineseToSimplified(traditionalChinese) !=
        simplifiedChinese) {
        std::cerr << "FAIL: traditional Chinese conversion mismatch\n";
        return 1;
    }
#else
    if (Platform::TextEncoding::ConvertTraditionalChineseToSimplified(traditionalChinese) !=
        traditionalChinese) {
        std::cerr << "FAIL: non-Windows traditional Chinese compatibility behavior changed\n";
        return 1;
    }
#endif

    const std::wstring diagnosticWide =
        L"Install script not found at: C:\\工作 区\\音效-🔊.ps1";
    const std::string diagnosticUtf8 =
        u8"Install script not found at: C:\\工作 区\\音效-🔊.ps1";
    if (Platform::TextEncoding::WideToUtf8Permissive(diagnosticWide) !=
        diagnosticUtf8) {
        std::cerr << "FAIL: diagnostic-shaped Unicode path changed\n";
        return 1;
    }

    std::wstring malformedDiagnostic = L"Install path: ";
    malformedDiagnostic.push_back(static_cast<wchar_t>(0xD800));
    malformedDiagnostic += L".dll";
    const std::string replacedDiagnostic = "Install path: " + replacement + ".dll";
    if (Platform::TextEncoding::WideToUtf8Permissive(malformedDiagnostic) !=
        replacedDiagnostic) {
        std::cerr << "FAIL: malformed diagnostic path was not replaced\n";
        return 1;
    }

    const wchar_t embeddedDiagnostic[] = L"Install path: C:\\音效\0hidden.dll";
    if (!Platform::TextEncoding::WideToUtf8Permissive(std::wstring(
            embeddedDiagnostic,
            sizeof(embeddedDiagnostic) / sizeof(embeddedDiagnostic[0]) - 1)).empty()) {
        std::cerr << "FAIL: embedded-NUL diagnostic path was not rejected\n";
        return 1;
    }

    const std::string nonAsciiUpper = std::string("R") + "\xC3\x89" + "APER";
    const std::string nonAsciiLower = std::string("r") + "\xC3\xA9" + "aper";
    if (!Platform::TextEncoding::EqualsAsciiCaseInsensitive("ReApEr.ExE", "reaper.exe") ||
        Platform::TextEncoding::EqualsAsciiCaseInsensitive("reaper.exe", "reaper.exe ") ||
        Platform::TextEncoding::EqualsAsciiCaseInsensitive("reper.exe", "reaper.exe") ||
        Platform::TextEncoding::EqualsAsciiCaseInsensitive(nonAsciiUpper, nonAsciiLower)) {
        std::cerr << "FAIL: ASCII case-insensitive comparison mismatch\n";
        return 1;
    }

    const std::string mojibakeChinese(
        "\xE6\xB6\x93\xEE\x85\x9F\xE6\x9E\x83", 9);
    const std::string utf8Chinese("\xE4\xB8\xAD\xE6\x96\x87", 6);
    const std::string malformedAnsiCandidate("\xC3\x28", 2);
#ifdef _WIN32
    if (Platform::TextEncoding::RepairUtf8MojibakeFromCp936(mojibakeChinese) !=
            utf8Chinese ||
        Platform::TextEncoding::RepairUtf8MojibakeFromCp936(utf8Chinese) !=
            utf8Chinese ||
        Platform::TextEncoding::RepairUtf8MojibakeFromCp936(
            malformedAnsiCandidate) != malformedAnsiCandidate) {
        std::cerr << "FAIL: CP936 mojibake repair contract mismatch\n";
        return 1;
    }
    const UINT systemAnsiCodePage = GetACP();
    bool systemAnsiRepairMismatch = false;
    if (systemAnsiCodePage == 936) {
        systemAnsiRepairMismatch =
            Platform::TextEncoding::RepairUtf8MojibakeFromSystemAnsi(
                mojibakeChinese) != utf8Chinese;
    } else if (systemAnsiCodePage == 1252) {
        const std::string westernMojibake("\xC3\x83\xC2\xA9", 4);
        const std::string westernUtf8("\xC3\xA9", 2);
        systemAnsiRepairMismatch =
            Platform::TextEncoding::Utf8ToWideStrict(westernMojibake) !=
                L"\u00C3\u00A9" ||
            Platform::TextEncoding::Utf8ToWideStrict(westernUtf8) != L"\u00E9" ||
            Platform::TextEncoding::RepairUtf8MojibakeFromSystemAnsi(
                westernMojibake) != westernUtf8;
    }
    if (systemAnsiRepairMismatch ||
        Platform::TextEncoding::RepairUtf8MojibakeFromSystemAnsi(utf8Chinese) !=
            utf8Chinese ||
        Platform::TextEncoding::RepairUtf8MojibakeFromSystemAnsi(malformedAnsiCandidate) !=
            malformedAnsiCandidate) {
        std::cerr << "FAIL: system ANSI mojibake repair contract mismatch\n";
        return 1;
    }
    if (Platform::TextEncoding::WideCharToCp936(L'\u4E2D') !=
            std::string("\xD6\xD0", 2) ||
        !Platform::TextEncoding::WideCharToCp936(
             static_cast<wchar_t>(0xD800)).empty()) {
        std::cerr << "FAIL: CP936 single-character encoding contract mismatch\n";
        return 1;
    }
#else
    if (Platform::TextEncoding::RepairUtf8MojibakeFromCp936(mojibakeChinese) !=
            mojibakeChinese ||
        Platform::TextEncoding::RepairUtf8MojibakeFromSystemAnsi(mojibakeChinese) !=
            mojibakeChinese ||
        !Platform::TextEncoding::WideCharToCp936(L'\u4E2D').empty()) {
        std::cerr << "FAIL: non-Windows ANSI compatibility behavior changed\n";
        return 1;
    }
#endif

#ifdef _WIN32
    const std::string invalidUtf8("\xC3\x28", 2);
    if (!Platform::TextEncoding::Utf8ToWideStrict(invalidUtf8).empty()) {
        std::cerr << "FAIL: invalid UTF-8 was accepted\n";
        return 1;
    }

    if (!Platform::TextEncoding::WideToUtf8Strict(loneSurrogate).empty()) {
        std::cerr << "FAIL: invalid wide input was accepted\n";
        return 1;
    }
#endif

    std::cout << "PASS: stage6 strict text encoding contracts\n";
    return 0;
}
