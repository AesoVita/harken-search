// 自定义音频数据源：包装后端解码器，读 PCM 后做中心感 DSP 再输出。
// 设计见 docs/plans/2026-03-17-中心感旋钮-design.md

#pragma once

#include "platform/audio_data_source.h"

enum class PitchMode : int;  // Forward declaration (defined in AudioEngine.h)
namespace CenterSenseDsp { class MagicEQSettingsPublisher; }

// 包装后端解码器，在 read 时对立体声做 processCenterSense 和 FreqShift DSP。
// getDataSource() 返回可交给播放后端的数据源地址。
// @stage7-contract
// API contract: CenterSenseDataSource borrows the backend SourceRef and
// optional DSP parameter pointers for the lifetime of an initialized stream;
// callers must keep them valid until uninit. It owns only its implementation
// state, is non-copyable/thread-confined, and returns a non-owning backend
// handle from getDataSource while preserving init/uninit failure semantics.
class CenterSenseDataSource {
public:
    struct SourceRef {
        SourceRef() noexcept
            : borrowed(Platform::AudioDataSource::BorrowDataSource(nullptr)) {}
        SourceRef(Platform::AudioDataSource::BorrowedSource source) noexcept
            : borrowed(source) {}

    private:
        Platform::AudioDataSource::BorrowedSource borrowed;
        friend class CenterSenseDataSource;
    };

    CenterSenseDataSource();
    ~CenterSenseDataSource();
    CenterSenseDataSource(const CenterSenseDataSource&) = delete;
    CenterSenseDataSource& operator=(const CenterSenseDataSource&) = delete;
    CenterSenseDataSource(CenterSenseDataSource&&) = delete;
    CenterSenseDataSource& operator=(CenterSenseDataSource&&) = delete;

    // freqShiftHzPtr / pitchModePtr: optional pointers for FreqShift mode.
    // When *pitchModePtr == PitchMode::FreqShift and |*freqShiftHzPtr| >= 0.1,
    // applies SSB Hilbert frequency shifting in the DSP chain.
    bool init(SourceRef source, unsigned int sampleRateHz,
              float const* centerSensePtr,
              float const* bassMonoFreqPtr  = nullptr,
              float const* freqShiftHzPtr   = nullptr,
              PitchMode const* pitchModePtr = nullptr,
              CenterSenseDsp::MagicEQSettingsPublisher const* magicEQSettings = nullptr);
    void uninit();

    // 返回可传给播放后端的数据源地址。
    void* getDataSource();

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
