#ifdef _WIN32

#include "image_codec.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <combaseapi.h>

namespace Platform::ImageCodec {

DecodedImage DecodeImageFromMemory(const uint8_t* data, size_t dataSize) {
    DecodedImage result;
    if (!data || dataSize == 0) return result;

    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (comHr == RPC_E_CHANGED_MODE)
        comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool needComUninit = (comHr == S_OK);

    IWICImagingFactory* pFactory = nullptr;
    IWICStream* pStream = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { if (needComUninit) CoUninitialize(); return result; }

    hr = pFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) hr = pStream->InitializeFromMemory(const_cast<BYTE*>(data), (DWORD)dataSize);
    if (SUCCEEDED(hr)) hr = pFactory->CreateDecoderFromStream(pStream, nullptr,
                                                               WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pFrame);
    if (SUCCEEDED(hr)) hr = pFactory->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) {
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0,
                                    WICBitmapPaletteTypeCustom);
    }

    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) hr = pConverter->GetSize(&w, &h);
    if (SUCCEEDED(hr) && w > 0 && h > 0) {
        result.width = (int)w;
        result.height = (int)h;
        size_t stride = (size_t)w * 4;
        result.rgba.resize(stride * (size_t)h);
        hr = pConverter->CopyPixels(nullptr, (UINT)stride, (UINT)result.rgba.size(), result.rgba.data());
        if (FAILED(hr)) {
            result.rgba.clear();
            result.width = result.height = 0;
        }
    }

    if (pConverter) pConverter->Release();
    if (pFrame) pFrame->Release();
    if (pDecoder) pDecoder->Release();
    if (pStream) pStream->Release();
    if (pFactory) pFactory->Release();
    if (needComUninit) CoUninitialize();
    return result;
}

std::vector<uint8_t> EncodeRgbaToJpeg(const uint8_t* rgbaPixels, int w, int h, float quality) {
    std::vector<uint8_t> outJpeg;
    if (!rgbaPixels || w <= 0 || h <= 0) return outJpeg;

    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (comHr == RPC_E_CHANGED_MODE)
        comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool needComUninit = (comHr == S_OK);

    IWICImagingFactory* pFactory = nullptr;
    IStream* pMemStream = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    IPropertyBag2* pProps = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { if (needComUninit) CoUninitialize(); return outJpeg; }

    pMemStream = SHCreateMemStream(nullptr, 0);
    if (!pMemStream) { pFactory->Release(); if (needComUninit) CoUninitialize(); return outJpeg; }

    hr = pFactory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &pEncoder);
    if (SUCCEEDED(hr)) hr = pEncoder->Initialize(pMemStream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = pEncoder->CreateNewFrame(&pFrame, &pProps);
    if (SUCCEEDED(hr)) {
        PROPBAG2 opt = {};
        opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT val;
        VariantInit(&val);
        val.vt = VT_R4;
        val.fltVal = quality;
        pProps->Write(1, &opt, &val);
        hr = pFrame->Initialize(pProps);
    }
    if (SUCCEEDED(hr)) hr = pFrame->SetSize((UINT)w, (UINT)h);
    WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) hr = pFrame->SetPixelFormat(&fmt);
    if (SUCCEEDED(hr)) {
        std::vector<uint8_t> bgr((size_t)w * (size_t)h * 3);
        for (int i = 0; i < w * h; ++i) {
            bgr[i * 3 + 0] = rgbaPixels[i * 4 + 2];
            bgr[i * 3 + 1] = rgbaPixels[i * 4 + 1];
            bgr[i * 3 + 2] = rgbaPixels[i * 4 + 0];
        }
        hr = pFrame->WritePixels((UINT)h, (UINT)(w * 3), (UINT)bgr.size(), bgr.data());
    }
    if (SUCCEEDED(hr)) hr = pFrame->Commit();
    if (SUCCEEDED(hr)) hr = pEncoder->Commit();

    if (SUCCEEDED(hr)) {
        STATSTG stat = {};
        if (SUCCEEDED(pMemStream->Stat(&stat, STATFLAG_NONAME))) {
            LARGE_INTEGER zero = {};
            pMemStream->Seek(zero, STREAM_SEEK_SET, nullptr);
            outJpeg.resize((size_t)stat.cbSize.QuadPart);
            ULONG bytesRead = 0;
            pMemStream->Read(outJpeg.data(), (ULONG)outJpeg.size(), &bytesRead);
            if (bytesRead != outJpeg.size()) outJpeg.clear();
        }
    }

    if (pFrame) pFrame->Release();
    if (pProps) pProps->Release();
    if (pEncoder) pEncoder->Release();
    if (pMemStream) pMemStream->Release();
    if (pFactory) pFactory->Release();
    if (needComUninit) CoUninitialize();
    return outJpeg;
}

} // namespace Platform::ImageCodec

#endif // _WIN32
