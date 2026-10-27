#pragma once

#include "../thirdparty/miniaudio.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Platform::AudioFile::TestSeam {

struct State {
    bool installed = false;
    std::vector<ma_result> initResults;
    std::vector<ma_result> readResults;
    std::vector<ma_uint64> readFrames;
    std::vector<ma_result> seekResults;
    std::vector<std::string> utf8Candidates;
    std::vector<std::wstring> wideCandidates;
    std::vector<ma_uint32> outputSampleRates;
    std::vector<char> callOrder;
    std::size_t initIndex = 0;
    std::size_t readIndex = 0;
    std::size_t seekIndex = 0;
    std::size_t readCallCount = 0;
    std::size_t seekCallCount = 0;
};

State& Get();
void Reset();
void Install();
void QueueInit(ma_result result);
void QueueRead(ma_result result, ma_uint64 framesRead);
void QueueSeek(ma_result result);

ma_result InitFile(const char* path, const ma_decoder_config* config, ma_decoder* decoder);
ma_result InitFileW(const wchar_t* path, const ma_decoder_config* config, ma_decoder* decoder);
ma_result ReadFrames(ma_decoder* decoder, void* destination, ma_uint64 requested,
                     ma_uint64* framesRead);
ma_result SeekFrame(ma_decoder* decoder, ma_uint64 frame);
void Uninit(ma_decoder* decoder);

} // namespace Platform::AudioFile::TestSeam
