#include "database_audio_probe_miniaudio_test_seam.h"

namespace Platform::DatabaseAudioProbe::TestSeam {
namespace {
State state;
void Record(char call) { if (state.callCount < state.calls.size()) state.calls[state.callCount++] = call; }
ma_result FinishInit(const ma_decoder_config* config, ma_decoder* decoder) {
    state.configFormat = config->format;
    state.configChannels = config->channels;
    state.configSampleRate = config->sampleRate;
    const ma_result result = state.initIndex < state.initCount ? state.initResults[state.initIndex++] : MA_ERROR;
    if (result == MA_SUCCESS) {
        decoder->outputSampleRate = state.outputSampleRate;
        decoder->outputChannels = state.outputChannels;
    }
    return result;
}
} // namespace
State& Get() { return state; }
void Reset() { state = {}; }
void Install() { state.installed = true; }
bool QueueInit(ma_result result) {
    if (state.initCount >= kCapacity) return false;
    state.initResults[state.initCount++] = result;
    return true;
}
bool QueueRead(ma_result result, ma_uint64 framesRead) {
    if (state.readCount >= kCapacity) return false;
    state.readResults[state.readCount] = result;
    state.readFrames[state.readCount++] = framesRead;
    return true;
}
ma_result InitWide(const wchar_t* path, const ma_decoder_config* config, ma_decoder* decoder) {
    if (!state.installed) return ma_decoder_init_file_w(path, config, decoder);
    Record('I');
    if (state.wideCount < kCapacity) state.wideCandidates[state.wideCount++] = path ? path : L"";
    return FinishInit(config, decoder);
}
ma_result InitNarrow(const char* path, const ma_decoder_config* config, ma_decoder* decoder) {
    if (!state.installed) return ma_decoder_init_file(path, config, decoder);
    Record('I');
    if (state.narrowCount < kCapacity) state.narrowCandidates[state.narrowCount++] = path ? path : "";
    return FinishInit(config, decoder);
}
ma_result GetLength(ma_decoder* decoder, ma_uint64* length) {
    if (!state.installed) return ma_decoder_get_length_in_pcm_frames(decoder, length);
    Record('L');
    *length = state.length;
    return state.lengthResult;
}
ma_result Read(ma_decoder* decoder, void* destination, ma_uint64 requested, ma_uint64* framesRead) {
    if (!state.installed) return ma_decoder_read_pcm_frames(decoder, destination, requested, framesRead);
    Record('R');
    if (state.requestCount < kCapacity) state.readRequests[state.requestCount++] = requested;
    if (state.readIndex >= state.readCount) { *framesRead = 0; return MA_ERROR; }
    *framesRead = state.readFrames[state.readIndex];
    return state.readResults[state.readIndex++];
}
void Uninit(ma_decoder* decoder) {
    if (!state.installed) { ma_decoder_uninit(decoder); return; }
    Record('U');
    ++state.uninitCount;
}
} // namespace Platform::DatabaseAudioProbe::TestSeam
