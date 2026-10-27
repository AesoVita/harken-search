#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Platform::ImageCodec {

// @stage7-contract
// API contract: DecodeImageFromMemory and EncodeRgbaToJpeg borrow input
// buffers only for the duration of the synchronous call and return owned
// vectors. Inputs must be non-null with valid dimensions/size; malformed data
// yields an empty result. No global state is changed and callers own all
// returned pixels.

struct DecodedImage {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
};

DecodedImage DecodeImageFromMemory(const uint8_t* data, size_t dataSize);

std::vector<uint8_t> EncodeRgbaToJpeg(const uint8_t* rgbaPixels, int w, int h,
                                      float quality = 0.82f);

} // namespace Platform::ImageCodec
