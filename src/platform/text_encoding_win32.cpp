#ifdef _WIN32

#include "text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <limits>

namespace {

std::wstring CodePageToWide(
    UINT codePage, DWORD flags, std::string_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    const int expected = MultiByteToWideChar(
        codePage, flags, value.data(), inputSize, nullptr, 0);
    if (expected <= 0) return {};
    std::wstring result(static_cast<std::size_t>(expected), L'\0');
    if (MultiByteToWideChar(
            codePage, flags, value.data(), inputSize, result.data(), expected) != expected) {
        return {};
    }
    return result;
}

std::string WideToCodePage(
    UINT codePage, std::wstring_view value, bool rejectDefaultCharacter) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    BOOL defaultCharacterUsed = FALSE;
    BOOL* usedDefault = rejectDefaultCharacter ? &defaultCharacterUsed : nullptr;
    const int expected = WideCharToMultiByte(
        codePage, 0, value.data(), inputSize, nullptr, 0, nullptr, usedDefault);
    if (expected <= 0 || defaultCharacterUsed) return {};
    std::string result(static_cast<std::size_t>(expected), '\0');
    defaultCharacterUsed = FALSE;
    const int converted = WideCharToMultiByte(
        codePage, 0, value.data(), inputSize, result.data(), expected, nullptr, usedDefault);
    if (converted != expected || defaultCharacterUsed) return {};
    return result;
}

} // namespace

namespace Platform::TextEncoding {

std::wstring Utf8ToWideStrict(std::string_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    const int expected = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputSize, nullptr, 0);
    if (expected <= 0) return {};
    std::wstring result(static_cast<std::size_t>(expected), L'\0');
    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), inputSize,
        result.data(), expected);
    if (converted != expected) return {};
    return result;
}

std::string WideToUtf8Strict(std::wstring_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    const int expected = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputSize,
        nullptr, 0, nullptr, nullptr);
    if (expected <= 0) return {};
    std::string result(static_cast<std::size_t>(expected), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputSize,
        result.data(), expected, nullptr, nullptr);
    if (converted != expected) return {};
    return result;
}

std::wstring Utf8ToWidePermissive(std::string_view value) {
    if (value.empty() || value.find('\0') != std::string_view::npos ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    const int expected = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), inputSize, nullptr, 0);
    if (expected <= 0) return {};
    std::wstring result(static_cast<std::size_t>(expected), L'\0');
    const int converted = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), inputSize, result.data(), expected);
    if (converted != expected) return {};
    return result;
}

std::string WideToUtf8Permissive(std::wstring_view value) {
    if (value.empty() || value.find(L'\0') != std::wstring_view::npos ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int inputSize = static_cast<int>(value.size());
    const int expected = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), inputSize, nullptr, 0, nullptr, nullptr);
    if (expected <= 0) return {};
    std::string result(static_cast<std::size_t>(expected), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), inputSize, result.data(), expected, nullptr, nullptr);
    if (converted != expected) return {};
    return result;
}

std::string RepairUtf8MojibakeFromSystemAnsi(std::string_view value) {
    if (value.empty() || value.find('\0') != std::string_view::npos) {
        return std::string(value);
    }
    const std::wstring wideValue = Utf8ToWidePermissive(value);
    if (wideValue.empty()) return std::string(value);

    const std::string ansiBytes = WideToCodePage(CP_ACP, wideValue, false);
    const std::wstring repairedWide =
        CodePageToWide(CP_UTF8, MB_ERR_INVALID_CHARS, ansiBytes);
    if (repairedWide.size() <= 1) return std::string(value);

    const std::string repairedUtf8 = WideToUtf8Strict(repairedWide);
    if (repairedUtf8.empty() || repairedUtf8 == value) return std::string(value);

    const std::wstring verifyWide =
        CodePageToWide(CP_ACP, MB_ERR_INVALID_CHARS, repairedUtf8);
    return verifyWide.size() > 1 && verifyWide == wideValue
        ? repairedUtf8
        : std::string(value);
}

std::string RepairUtf8MojibakeFromCp936(std::string_view value) {
    if (value.empty() || value.find('\0') != std::string_view::npos) {
        return std::string(value);
    }
    const std::wstring wideValue = Utf8ToWidePermissive(value);
    if (wideValue.empty()) return std::string(value);

    const std::string cp936Bytes = WideToCodePage(936, wideValue, false);
    const std::wstring repairedWide =
        CodePageToWide(CP_UTF8, MB_ERR_INVALID_CHARS, cp936Bytes);
    if (repairedWide.size() <= 1) return std::string(value);

    const std::string repairedUtf8 = WideToUtf8Strict(repairedWide);
    if (repairedUtf8.empty() || repairedUtf8 == value) return std::string(value);

    const std::wstring verifyWide =
        CodePageToWide(936, MB_ERR_INVALID_CHARS, repairedUtf8);
    return verifyWide.size() > 1 && verifyWide == wideValue
        ? repairedUtf8
        : std::string(value);
}

std::string WideCharToCp936(wchar_t value) {
    return WideToCodePage(936, std::wstring_view(&value, 1), true);
}

std::string ConvertTraditionalChineseToSimplified(std::string_view value) {
    if (value.empty()) return std::string(value);
    const std::wstring wide = Utf8ToWidePermissive(value);
    if (wide.empty() ||
        wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return std::string(value);
    }
    const int inputSize = static_cast<int>(wide.size());
    const int expected = LCMapStringEx(
        L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), inputSize,
        nullptr, 0, nullptr, nullptr, 0);
    if (expected <= 0) return std::string(value);
    std::wstring simplified(static_cast<std::size_t>(expected), L'\0');
    const int written = LCMapStringEx(
        L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, wide.data(), inputSize,
        simplified.data(), expected, nullptr, nullptr, 0);
    if (written <= 0) return std::string(value);
    const std::string result = WideToUtf8Permissive(simplified);
    return result.empty() ? std::string(value) : result;
}

bool UppercaseWindowsInvariantUtf8(std::string_view value, std::string& output, std::wstring& wideScratch, std::wstring& uppercaseScratch) {
    if (value.empty()) {
        output.clear();
        return true;
    }
    if (value.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    const int sourceLength = static_cast<int>(value.size());
    const int wideLength = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), sourceLength, nullptr, 0);
    if (wideLength <= 0) return false;
    wideScratch.resize(static_cast<std::size_t>(wideLength));
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), sourceLength,
            wideScratch.data(), wideLength) != wideLength) {
        return false;
    }

    const int uppercaseLength = LCMapStringEx(
        LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
        wideScratch.data(), wideLength,
        nullptr, 0, nullptr, nullptr, 0);
    if (uppercaseLength <= 0) return false;
    uppercaseScratch.resize(static_cast<std::size_t>(uppercaseLength));
    if (LCMapStringEx(
            LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
            wideScratch.data(), wideLength,
            uppercaseScratch.data(), uppercaseLength,
            nullptr, nullptr, 0) != uppercaseLength) {
        return false;
    }

    const int utf8Length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        uppercaseScratch.data(), uppercaseLength,
        nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) return false;
    output.resize(static_cast<std::size_t>(utf8Length));
    return WideCharToMultiByte(
               CP_UTF8, WC_ERR_INVALID_CHARS,
               uppercaseScratch.data(), uppercaseLength,
               output.data(), utf8Length, nullptr, nullptr) == utf8Length;
}

} // namespace Platform::TextEncoding

#endif // _WIN32
