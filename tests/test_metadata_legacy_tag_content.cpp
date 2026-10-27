#include "platform/metadata_legacy_tag_content.h"
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>

using Platform::Metadata::LegacyTagContent::DecodeId3v2Encoding0;
using Platform::Metadata::LegacyTagContent::DecodeUtf16;
using Platform::Metadata::LegacyTagContent::Utf16ByteOrder;

namespace {
int failures = 0;
void Check(bool ok, const char* message) {
    if (!ok) { ++failures; std::cerr << message << '\n'; }
}
std::string Bytes(std::initializer_list<unsigned char> values) {
    return std::string(reinterpret_cast<const char*>(values.begin()), values.size());
}
}

int main() {
    Check(DecodeId3v2Encoding0({}).empty(), "encoding-0 empty");
    const char embedded[] = {'P', '\0', 'H'};
    Check(DecodeId3v2Encoding0({embedded, 3}) == std::string(embedded, 3), "encoding-0 NUL");
    const uint8_t le[] = {'L', 0, 'E', 0, 0, 0, 'X', 0};
    const uint8_t beOdd[] = {0, 'B', 0, 'E', 0xFF};
    Check(DecodeUtf16(le, sizeof(le), Utf16ByteOrder::LittleEndian) == "LE", "LE NUL");
    Check(DecodeUtf16(beOdd, sizeof(beOdd), Utf16ByteOrder::BigEndian) == "BE", "BE odd");
    Check(DecodeUtf16(nullptr, 3, Utf16ByteOrder::LittleEndian).empty(), "null empty");
    const uint8_t pair[] = {0x3D, 0xD8, 0x00, 0xDE};
    const uint8_t lone[] = {0x3D, 0xD8};
#if defined(_WIN32)
    Check(DecodeId3v2Encoding0(Bytes({0x80})) == Bytes({0xE2, 0x82, 0xAC}), "CP1252 euro");
    Check(DecodeId3v2Encoding0(Bytes({0x93, 0x94})) == Bytes({0xE2, 0x80, 0x9C, 0xE2, 0x80, 0x9D}), "CP1252 quotes");
    Check(DecodeUtf16(pair, sizeof(pair), Utf16ByteOrder::LittleEndian) == Bytes({0xF0, 0x9F, 0x98, 0x80}), "UTF16 pair");
    Check(DecodeUtf16(lone, sizeof(lone), Utf16ByteOrder::LittleEndian) == Bytes({0xEF, 0xBF, 0xBD}), "UTF16 replacement");
#else
    Check(DecodeId3v2Encoding0(Bytes({0x80, 0x93, 0x94})) == Bytes({0xC2, 0x80, 0xC2, 0x93, 0xC2, 0x94}), "ISO bytes");
    Check(DecodeUtf16(pair, sizeof(pair), Utf16ByteOrder::LittleEndian) == Bytes({0xED, 0xA0, 0xBD, 0xED, 0xB8, 0x80}), "naive pair");
    Check(DecodeUtf16(lone, sizeof(lone), Utf16ByteOrder::LittleEndian) == Bytes({0xED, 0xA0, 0xBD}), "naive lone");
#endif
    return failures;
}
