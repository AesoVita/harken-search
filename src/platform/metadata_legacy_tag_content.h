#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Platform::Metadata::LegacyTagContent {

// @stage7-contract
// API contract: the decoding helpers synchronously consume caller-owned byte
// views and return newly-owned UTF-8 strings. Inputs may be empty; malformed
// sequences are represented by the existing replacement behavior. No caller
// buffer is retained, no global state is touched, and the functions do not
// throw across the metadata/platform boundary.

enum class Utf16ByteOrder : uint8_t { LittleEndian, BigEndian };

std::string DecodeId3v2Encoding0(std::string_view bytes);
std::string DecodeUtf16(const uint8_t* bytes, size_t size, Utf16ByteOrder order);

} // namespace Platform::Metadata::LegacyTagContent
