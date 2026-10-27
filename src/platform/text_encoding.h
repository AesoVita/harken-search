#pragma once

#include <string>
#include <string_view>

namespace Platform::TextEncoding {

// @stage7-contract
// API contract: conversion functions return owned strings and never retain the
// input view. Strict conversions report invalid input through their established
// empty-result behavior; permissive and repair helpers preserve their named
// loss-tolerance policy. The Windows invariant uppercase helper borrows its
// input and caller-owned reusable buffers without retaining them; output is
// valid only when it returns true. Unsupported or invalid input returns false.
// Functions are reentrant and have no shared state.

std::wstring Utf8ToWideStrict(std::string_view value);
std::string WideToUtf8Strict(std::wstring_view value);
std::wstring Utf8ToWidePermissive(std::string_view value);
std::string WideToUtf8Permissive(std::wstring_view value);
std::string RepairUtf8MojibakeFromSystemAnsi(std::string_view value);
std::string RepairUtf8MojibakeFromCp936(std::string_view value);
std::string WideCharToCp936(wchar_t value);
std::string ConvertTraditionalChineseToSimplified(std::string_view value);
bool UppercaseWindowsInvariantUtf8(std::string_view value, std::string& output, std::wstring& wideScratch, std::wstring& uppercaseScratch);
bool EqualsAsciiCaseInsensitive(std::string_view left, std::string_view right);

} // namespace Platform::TextEncoding
