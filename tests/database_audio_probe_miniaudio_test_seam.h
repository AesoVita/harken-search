#pragma once

#include "../thirdparty/miniaudio.h"

#include <array>
#include <string>

namespace Platform::DatabaseAudioProbe::TestSeam {
constexpr size_t kCapacity = 16;
struct State {
    bool installed = false;
    std::array<ma_result, kCapacity> initResults{};
    std::array<ma_result, kCapacity> readResults{};
    std::array<ma_uint64, kCapacity> readFrames{};
    std::array<ma_uint64, kCapacity> readRequests{};
    std::array<std::wstring, kCapacity> wideCandidates{};
    std::array<std::string, kCapacity> narrowCandidates{};
    std::array<char, kCapacity * 2> calls{};
    size_t initCount = 0, initIndex = 0, readCount = 0, readIndex = 0;
    size_t wideCount = 0, narrowCount = 0, requestCount = 0, callCount = 0, uninitCount = 0;
    ma_result lengthResult = MA_SUCCESS;
    ma_uint64 length = 0;
    ma_uint32 outputSampleRate = 0, outputChannels = 0;
    ma_format configFormat = ma_format_unknown;
    ma_uint32 configChannels = 0, configSampleRate = 0;
};
State& Get();
void Reset();
void Install();
bool QueueInit(ma_result result);
bool QueueRead(ma_result result, ma_uint64 framesRead);
ma_result InitWide(const wchar_t*, const ma_decoder_config*, ma_decoder*);
ma_result InitNarrow(const char*, const ma_decoder_config*, ma_decoder*);
ma_result GetLength(ma_decoder*, ma_uint64*);
ma_result Read(ma_decoder*, void*, ma_uint64, ma_uint64*);
void Uninit(ma_decoder*);
} // namespace Platform::DatabaseAudioProbe::TestSeam
