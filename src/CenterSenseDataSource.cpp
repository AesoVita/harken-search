// CenterSenseDataSource — 包装 decoder，read 时做中心感 DSP

#include "CenterSenseDataSource.h"
#include "CenterSenseDsp.h"
#include "AudioEngine.h"   // For PitchMode full definition
#include <cmath>

namespace {
struct TransformState {
    unsigned int sampleRate;
    float const* pCenterSense;
    CenterSenseDsp::ProcessState processState;
    float const* pBassMonoFreq;
    CenterSenseDsp::BassMonoState bassMonoState;
    float const* pFreqShiftHz = nullptr;
    PitchMode const* pPitchMode = nullptr;
    CenterSenseDsp::FreqShiftState freqShiftState;
    CenterSenseDsp::MagicEQSettingsPublisher const* pMagicEQSettings = nullptr;
    MagicEQSettings currentMagicEQ{};
    CenterSenseDsp::MagicEQState magicEQState;
};

} // namespace

struct CenterSenseDataSource::Impl {
    Platform::AudioDataSource::TransformSource source;
    TransformState transform;
};

namespace {
using Platform::AudioDataSource::FrameCount;
using Platform::AudioDataSource::ReadResult;
using Platform::AudioDataSource::Reader;

ReadResult ReadTransformed(void* context, Reader decoder, float* output,
                           FrameCount frameCount) {
    auto* impl = static_cast<TransformState*>(context);
    ReadResult result{};

    result = decoder.Read(output, frameCount);
    if (result.backendCode != 0) {
        result.framesRead = 0;
        return result;
    }
    if (result.framesRead == 0) return result;

    const unsigned int ch = decoder.Channels();
    if (ch == 1) {
        // Frequency shifter also applies to mono assets. Keep stereo-only
        // processors below out of the mono path.
        if (impl->pPitchMode && *impl->pPitchMode == PitchMode::FreqShift &&
            impl->pFreqShiftHz && std::abs(*impl->pFreqShiftHz) >= 0.1f) {
            CenterSenseDsp::processFreqShiftMono(
                output, static_cast<size_t>(result.framesRead), (int)impl->sampleRate,
                *impl->pFreqShiftHz, &impl->freqShiftState);
        }
    } else if (ch == 2) {
        // Bass mono first: collapse low frequencies to mono
        if (impl->pBassMonoFreq && *impl->pBassMonoFreq > 0.0f) {
            CenterSenseDsp::processBassMono(
                output, static_cast<size_t>(result.framesRead), (int)impl->sampleRate,
                *impl->pBassMonoFreq, &impl->bassMonoState);
        }
        // Frequency shifter (SSB Hilbert): only active in FreqShift mode
        if (impl->pPitchMode && *impl->pPitchMode == PitchMode::FreqShift &&
            impl->pFreqShiftHz && std::abs(*impl->pFreqShiftHz) >= 0.1f) {
            CenterSenseDsp::processFreqShift(
                output, static_cast<size_t>(result.framesRead), (int)impl->sampleRate,
                *impl->pFreqShiftHz, &impl->freqShiftState);
        }
        // CenterSense: stereo field control
        if (impl->pCenterSense && std::abs(*impl->pCenterSense) >= 1e-6f) {
            CenterSenseDsp::processCenterSense(
                output, static_cast<size_t>(result.framesRead), (int)impl->sampleRate,
                *impl->pCenterSense, &impl->processState);
        }
    }
    if (impl->pMagicEQSettings) {
        impl->pMagicEQSettings->tryLoad(impl->currentMagicEQ);
        CenterSenseDsp::MagicEQProcessParams params;
        params.enabled = impl->currentMagicEQ.enabled;
        params.mode = impl->currentMagicEQ.mode;
        params.steepness = impl->currentMagicEQ.steepness;
        params.centerFreq = impl->currentMagicEQ.centerFreq;
        params.halfWidthPercent = impl->currentMagicEQ.halfWidthPercent;
        CenterSenseDsp::processMagicEQ(output, static_cast<size_t>(result.framesRead),
                                       static_cast<int>(ch), static_cast<int>(impl->sampleRate),
                                       params, &impl->magicEQState);
    }
    return result;
}

void ResetAfterSeek(void* context) {
    auto* impl = static_cast<TransformState*>(context);
    impl->processState = CenterSenseDsp::ProcessState{};
    impl->bassMonoState = CenterSenseDsp::BassMonoState{};
    impl->freqShiftState = CenterSenseDsp::FreqShiftState{};
    CenterSenseDsp::resetMagicEQ(&impl->magicEQState);
}
} // namespace

CenterSenseDataSource::CenterSenseDataSource() = default;

CenterSenseDataSource::~CenterSenseDataSource() {
    uninit();
}

bool CenterSenseDataSource::init(SourceRef source, unsigned int sampleRateHz,
                                 float const* centerSensePtr,
                                 float const* bassMonoFreqPtr,
                                 float const* freqShiftHzPtr,
                                 PitchMode const* pitchModePtr,
                                 CenterSenseDsp::MagicEQSettingsPublisher const* magicEQSettings) {
    if (m_impl || !source.borrowed.IsValid() || !centerSensePtr) return false;
    auto* next = new Impl();
    next->transform.sampleRate = sampleRateHz;
    next->transform.pCenterSense = centerSensePtr;
    next->transform.pBassMonoFreq = bassMonoFreqPtr;
    next->transform.pFreqShiftHz = freqShiftHzPtr;
    next->transform.pPitchMode = pitchModePtr;
    next->transform.pMagicEQSettings = magicEQSettings;
    if (!next->source.Initialize(
            source.borrowed, {&next->transform, ReadTransformed, ResetAfterSeek})) {
        delete next;
        return false;
    }
    m_impl = next;
    return true;
}

void CenterSenseDataSource::uninit() {
    if (!m_impl) return;
    m_impl->source.Uninitialize();
    delete m_impl;
    m_impl = nullptr;
}

void* CenterSenseDataSource::getDataSource() {
    return m_impl ? m_impl->source.NativeHandle() : nullptr;
}
