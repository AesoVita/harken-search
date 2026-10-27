#ifdef __APPLE__

#include "image_codec.h"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

#include "stb_image.h"

namespace Platform::ImageCodec {

DecodedImage DecodeImageFromMemory(const uint8_t* data, size_t dataSize) {
    DecodedImage result;
    if (!data || dataSize == 0) return result;

    int w = 0, h = 0, channels = 0;
    uint8_t* pixels = stbi_load_from_memory(data, (int)dataSize, &w, &h, &channels, 4);
    if (pixels && w > 0 && h > 0) {
        result.width = w;
        result.height = h;
        result.rgba.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
    }
    if (pixels) stbi_image_free(pixels);
    return result;
}

std::vector<uint8_t> EncodeRgbaToJpeg(const uint8_t* rgbaPixels, int w, int h, float quality) {
    std::vector<uint8_t> outJpeg;
    if (!rgbaPixels || w <= 0 || h <= 0) return outJpeg;

    @autoreleasepool {
        size_t rgbaLen = (size_t)w * (size_t)h * 4;
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            (void*)rgbaPixels, w, h, 8, w * 4, colorSpace,
            kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(colorSpace);
        if (!ctx) return outJpeg;

        CGImageRef cgImage = CGBitmapContextCreateImage(ctx);
        CGContextRelease(ctx);
        if (!cgImage) return outJpeg;

        NSMutableData* jpegData = [NSMutableData data];
        CGImageDestinationRef dest = CGImageDestinationCreateWithData(
            (__bridge CFMutableDataRef)jpegData, kUTTypeJPEG, 1, nullptr);
        if (dest) {
            NSDictionary* props = @{
                (__bridge NSString*)kCGImageDestinationLossyCompressionQuality: @(quality)
            };
            CGImageDestinationAddImage(dest, cgImage, (__bridge CFDictionaryRef)props);
            CGImageDestinationFinalize(dest);
            CFRelease(dest);

            outJpeg.assign((uint8_t*)[jpegData bytes],
                           (uint8_t*)[jpegData bytes] + [jpegData length]);
        }
        CGImageRelease(cgImage);
    }
    return outJpeg;
}

} // namespace Platform::ImageCodec

#endif // __APPLE__
