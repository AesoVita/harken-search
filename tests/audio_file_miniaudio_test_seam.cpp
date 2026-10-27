#include "audio_file_miniaudio_test_seam.h"

#include <utility>

namespace Platform::AudioFile::TestSeam {
namespace {
State state;
}

State& Get() { return state; }
void Reset() { state = {}; }
void Install() { state.installed = true; }
void QueueInit(ma_result result) { state.initResults.push_back(result); }
void QueueRead(ma_result result, ma_uint64 framesRead) {
    state.readResults.push_back(result);
    state.readFrames.push_back(framesRead);
}
void QueueSeek(ma_result result) { state.seekResults.push_back(result); }

ma_result InitFile(const char* path, const ma_decoder_config* config, ma_decoder* decoder) {
    if (state.installed) state.callOrder.push_back('I');
    state.utf8Candidates.emplace_back(path ? path : "");
    state.outputSampleRates.push_back(config ? config->sampleRate : 0);
    if (!state.installed) return ma_decoder_init_file(path, config, decoder);
    if (state.initIndex >= state.initResults.size()) return MA_ERROR;
    return state.initResults[state.initIndex++];
}

ma_result InitFileW(const wchar_t* path, const ma_decoder_config* config, ma_decoder* decoder) {
    if (state.installed) state.callOrder.push_back('I');
    state.wideCandidates.emplace_back(path ? path : L"");
    state.outputSampleRates.push_back(config ? config->sampleRate : 0);
    if (!state.installed) return ma_decoder_init_file_w(path, config, decoder);
    if (state.initIndex >= state.initResults.size()) return MA_ERROR;
    return state.initResults[state.initIndex++];
}

ma_result ReadFrames(ma_decoder* decoder, void* destination, ma_uint64 requested,
                     ma_uint64* framesRead) {
    if (!state.installed)
        return ma_decoder_read_pcm_frames(decoder, destination, requested, framesRead);
    state.callOrder.push_back('R');
    ++state.readCallCount;
    if (state.readIndex >= state.readResults.size()) {
        if (framesRead) *framesRead = 0;
        return MA_ERROR;
    }
    if (framesRead) *framesRead = state.readFrames[state.readIndex];
    return state.readResults[state.readIndex++];
}

ma_result SeekFrame(ma_decoder* decoder, ma_uint64 frame) {
    if (!state.installed) return ma_decoder_seek_to_pcm_frame(decoder, frame);
    state.callOrder.push_back('S');
    ++state.seekCallCount;
    if (state.seekIndex >= state.seekResults.size()) return MA_ERROR;
    return state.seekResults[state.seekIndex++];
}

void Uninit(ma_decoder* decoder) {
    if (!state.installed) ma_decoder_uninit(decoder);
}

} // namespace Platform::AudioFile::TestSeam
