#include "../MediaFoundationAudioDecoder.h"
#include "text_encoding.h"

#include <cstdio>
#include <limits>
#include <mutex>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef CLSCTX_ALL
#undef CLSCTX_ALL
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>

namespace HarkenSearch::MediaFoundationAudio {

namespace detail {

struct ComApartment {
    HRESULT hr = E_FAIL;
    bool uninit = false;

    ComApartment();
    ~ComApartment();
    bool ok() const;
};

std::string HrToString(HRESULT hr) {
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), "hr=0x%08lX", (unsigned long)hr);
    return std::string(buf);
}

bool EnsureStarted(std::string* error) {
    static std::once_flag once;
    static HRESULT startupHr = E_FAIL;
    std::call_once(once, []() {
        startupHr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    });
    if (FAILED(startupHr)) {
        if (error) *error = HrToString(startupHr);
        return false;
    }
    return true;
}

ComApartment::ComApartment() {
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    uninit = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        hr = S_OK;
        uninit = false;
    }
}

ComApartment::~ComApartment() {
    if (uninit) CoUninitialize();
}

bool ComApartment::ok() const {
    return SUCCEEDED(hr);
}

bool CreateReader(const std::string& path,
                  Microsoft::WRL::ComPtr<IMFSourceReader>& reader,
                  std::string* error) {
    std::string startupError;
    if (!EnsureStarted(&startupError)) {
        if (error) *error = startupError;
        return false;
    }

    Microsoft::WRL::ComPtr<IMFAttributes> attrs;
    HRESULT hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }
    attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, FALSE);

    std::wstring widePath = ToMediaFoundationPath(Platform::TextEncoding::Utf8ToWidePermissive(path));
    hr = MFCreateSourceReaderFromURL(widePath.c_str(), attrs.Get(), &reader);
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }

    Microsoft::WRL::ComPtr<IMFMediaType> type;
    hr = MFCreateMediaType(&type);
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
    hr = reader->SetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get());
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }

    return true;
}

bool GetCurrentAudioFormat(IMFSourceReader* reader,
                           uint32_t& sampleRate,
                           uint32_t& channels,
                           GUID& subtype,
                           uint32_t& bitsPerSample,
                           std::string* error) {
    sampleRate = 0;
    channels = 0;
    subtype = GUID_NULL;
    bitsPerSample = 0;

    Microsoft::WRL::ComPtr<IMFMediaType> type;
    HRESULT hr = reader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type);
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }
    UINT32 value = 0;
    if (SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &value))) {
        sampleRate = value;
    }
    if (SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &value))) {
        channels = value;
    }
    if (SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &value))) {
        bitsPerSample = value;
    }
    type->GetGUID(MF_MT_SUBTYPE, &subtype);
    if (sampleRate == 0 || channels == 0) {
        if (error) *error = "missing_audio_format";
        return false;
    }
    return true;
}

bool AppendBufferAsFloat(IMFMediaBuffer* buffer,
                         GUID subtype,
                         uint32_t bitsPerSample,
                         std::vector<float>& out,
                         std::string* error) {
    BYTE* data = nullptr;
    DWORD maxLen = 0;
    DWORD currentLen = 0;
    HRESULT hr = buffer->Lock(&data, &maxLen, &currentLen);
    if (FAILED(hr)) {
        if (error) *error = HrToString(hr);
        return false;
    }

    if (subtype == MFAudioFormat_Float) {
        const size_t count = currentLen / sizeof(float);
        const float* src = reinterpret_cast<const float*>(data);
        out.insert(out.end(), src, src + count);
    } else if (subtype == MFAudioFormat_PCM && bitsPerSample == 16) {
        const size_t count = currentLen / sizeof(int16_t);
        const int16_t* src = reinterpret_cast<const int16_t*>(data);
        out.reserve(out.size() + count);
        for (size_t i = 0; i < count; ++i) {
            out.push_back((float)src[i] / 32768.0f);
        }
    } else if (subtype == MFAudioFormat_PCM && bitsPerSample == 32) {
        const size_t count = currentLen / sizeof(int32_t);
        const int32_t* src = reinterpret_cast<const int32_t*>(data);
        out.reserve(out.size() + count);
        constexpr double scale = 2147483648.0;
        for (size_t i = 0; i < count; ++i) {
            out.push_back((float)((double)src[i] / scale));
        }
    } else {
        buffer->Unlock();
        if (error) *error = "unsupported_pcm_subtype";
        return false;
    }

    buffer->Unlock();
    return true;
}

} // namespace detail

bool Probe(const std::string& path,
           uint32_t& sampleRate,
           uint32_t& channels,
           uint64_t& totalFrames,
           float& durationSeconds,
           std::string* error) {
    sampleRate = 0;
    channels = 0;
    totalFrames = 0;
    durationSeconds = 0.0f;
    if (!CanDecodeExtension(path)) {
        if (error) *error = "media_foundation_extension_not_supported";
        return false;
    }

    detail::ComApartment apartment;
    if (!apartment.ok()) {
        if (error) *error = ClassifyFailure(detail::HrToString(apartment.hr));
        return false;
    }

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    std::string detailError;
    if (!detail::CreateReader(path, reader, &detailError)) {
        if (error) *error = ClassifyFailure(detailError);
        return false;
    }

    GUID subtype = GUID_NULL;
    uint32_t bits = 0;
    if (!detail::GetCurrentAudioFormat(
            reader.Get(), sampleRate, channels, subtype, bits, &detailError)) {
        if (error) *error = ClassifyFailure(detailError);
        return false;
    }

    PROPVARIANT var;
    PropVariantInit(&var);
    HRESULT hr = reader->GetPresentationAttribute(
        MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var);
    if (SUCCEEDED(hr) && var.vt == VT_UI8 && var.uhVal.QuadPart > 0) {
        const double seconds = (double)var.uhVal.QuadPart / 10000000.0;
        durationSeconds = (float)seconds;
        totalFrames = (uint64_t)std::llround(seconds * (double)sampleRate);
    }
    PropVariantClear(&var);

    if (durationSeconds <= 0.0f || totalFrames == 0) {
        if (error) *error = ClassifyFailure("missing_duration");
        return false;
    }
    return true;
}

bool Decode(const std::string& path,
            DecodedAudioBuffer& out,
            std::string* error,
            const std::atomic<bool>* cancelFlag) {
    out = {};
    if (!CanDecodeExtension(path)) {
        if (error) *error = "media_foundation_extension_not_supported";
        return false;
    }

    detail::ComApartment apartment;
    if (!apartment.ok()) {
        if (error) *error = ClassifyFailure(detail::HrToString(apartment.hr));
        return false;
    }

    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    std::string detailError;
    if (!detail::CreateReader(path, reader, &detailError)) {
        if (error) *error = ClassifyFailure(detailError);
        return false;
    }

    GUID subtype = GUID_NULL;
    uint32_t bits = 0;
    if (!detail::GetCurrentAudioFormat(
            reader.Get(), out.sampleRate, out.channels, subtype, bits, &detailError)) {
        if (error) *error = ClassifyFailure(detailError);
        return false;
    }

    uint32_t probeSr = 0;
    uint32_t probeCh = 0;
    uint64_t probeFrames = 0;
    float probeDuration = 0.0f;
    Probe(path, probeSr, probeCh, probeFrames, probeDuration, nullptr);
    if (probeFrames > 0 && probeCh > 0) {
        const uint64_t reserveSamples = probeFrames * (uint64_t)probeCh;
        if (reserveSamples < (uint64_t)std::numeric_limits<size_t>::max() / sizeof(float)) {
            out.interleaved.reserve((size_t)reserveSamples);
        }
    }

    for (;;) {
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
            if (error) *error = "cancelled";
            return false;
        }
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        Microsoft::WRL::ComPtr<IMFSample> sample;
        HRESULT hr = reader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &sample);
        if (FAILED(hr)) {
            if (error) *error = ClassifyFailure(detail::HrToString(hr));
            return false;
        }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            if (!detail::GetCurrentAudioFormat(
                    reader.Get(), out.sampleRate, out.channels, subtype, bits, &detailError)) {
                if (error) *error = ClassifyFailure(detailError);
                return false;
            }
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) {
            if (error) *error = ClassifyFailure(detail::HrToString(hr));
            return false;
        }
        if (!detail::AppendBufferAsFloat(
                buffer.Get(), subtype, bits, out.interleaved, &detailError)) {
            if (error) *error = ClassifyFailure(detailError);
            return false;
        }
    }

    if (out.channels == 0 || out.sampleRate == 0 || out.interleaved.empty()) {
        if (error) *error = ClassifyFailure("decode_no_frames");
        return false;
    }
    out.totalFrames = (uint64_t)(out.interleaved.size() / (size_t)out.channels);
    out.durationSeconds = (out.sampleRate > 0)
        ? (float)((double)out.totalFrames / (double)out.sampleRate)
        : 0.0f;
    if (out.totalFrames == 0) {
        if (error) *error = ClassifyFailure("decode_no_frames");
        return false;
    }
    return true;
}

} // namespace HarkenSearch::MediaFoundationAudio
