#ifndef _WIN32

#include "text_encoding.h"

#include <codecvt>
#include <cstdint>
#include <limits>
#include <locale>
#include <stdexcept>

namespace {

constexpr std::uint32_t kReplacement = 0xFFFD;

void AppendWide(std::wstring& output, std::uint32_t codePoint) {
    if constexpr (sizeof(wchar_t) == 2) {
        if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<wchar_t>(codePoint));
        } else {
            codePoint -= 0x10000;
            output.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
            output.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
        }
    } else {
        output.push_back(static_cast<wchar_t>(codePoint));
    }
}

void AppendUtf8(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

} // namespace

namespace Platform::TextEncoding {

std::wstring Utf8ToWideStrict(std::string_view value) {
    if (value.empty()) return {};
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(value.data(), value.data() + value.size());
    } catch (const std::range_error&) {
        return {};
    }
}

std::string WideToUtf8Strict(std::wstring_view value) {
    if (value.empty()) return {};
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(value.data(), value.data() + value.size());
    } catch (const std::range_error&) {
        return {};
    }
}

std::wstring Utf8ToWidePermissive(std::string_view value) {
    if (value.empty() || value.find('\0') != std::string_view::npos ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    try {
        std::wstring result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size();) {
            const auto first = static_cast<unsigned char>(value[index]);
            std::uint32_t codePoint = kReplacement;
            std::size_t length = 1;
            std::uint32_t minimum = 0;
            if (first <= 0x7F) {
                codePoint = first;
            } else if (first >= 0xC2 && first <= 0xDF) {
                codePoint = first & 0x1F;
                length = 2;
                minimum = 0x80;
            } else if (first >= 0xE0 && first <= 0xEF) {
                codePoint = first & 0x0F;
                length = 3;
                minimum = 0x800;
            } else if (first >= 0xF0 && first <= 0xF4) {
                codePoint = first & 0x07;
                length = 4;
                minimum = 0x10000;
            }
            bool valid = codePoint != kReplacement && index + length <= value.size();
            for (std::size_t offset = 1; valid && offset < length; ++offset) {
                const auto continuation = static_cast<unsigned char>(value[index + offset]);
                if ((continuation & 0xC0) != 0x80) {
                    valid = false;
                } else {
                    codePoint = (codePoint << 6) | (continuation & 0x3F);
                }
            }
            if (!valid || codePoint < minimum || codePoint > 0x10FFFF ||
                (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
                AppendWide(result, kReplacement);
                ++index;
            } else {
                AppendWide(result, codePoint);
                index += length;
            }
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::string WideToUtf8Permissive(std::wstring_view value) {
    if (value.empty() || value.find(L'\0') != std::wstring_view::npos ||
        value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    try {
        std::string result;
        result.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index) {
            std::uint32_t codePoint = static_cast<std::uint32_t>(value[index]);
            if constexpr (sizeof(wchar_t) == 2) {
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
                    index + 1 < value.size()) {
                    const auto low = static_cast<std::uint32_t>(value[index + 1]);
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                        ++index;
                    } else {
                        codePoint = kReplacement;
                    }
                } else if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
                    codePoint = kReplacement;
                }
            } else if (codePoint > 0x10FFFF ||
                       (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
                codePoint = kReplacement;
            }
            AppendUtf8(result, codePoint);
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::string RepairUtf8MojibakeFromSystemAnsi(std::string_view value) {
    return std::string(value);
}

std::string RepairUtf8MojibakeFromCp936(std::string_view value) {
    return std::string(value);
}

std::string WideCharToCp936(wchar_t) {
    return {};
}

std::string ConvertTraditionalChineseToSimplified(std::string_view value) {
    return std::string(value);
}

bool UppercaseWindowsInvariantUtf8(std::string_view, std::string&, std::wstring&, std::wstring&) {
    return false;
}

} // namespace Platform::TextEncoding

#endif // !_WIN32
