#include "database_audio_probe_miniaudio_test_seam.h"
#include "platform/database_audio_probe_backend.h"

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;
using namespace Platform::DatabaseAudioProbe;
using namespace Platform::DatabaseAudioProbe::TestSeam;
namespace {
int failures = 0;
void Check(bool ok, const char* message) { if (!ok) { std::printf("FAIL: %s\n", message); ++failures; } }
void Setup(ma_result initResult, ma_uint64 length = 44100) {
    Reset(); Install(); auto& state = Get(); state.outputSampleRate = 44100; state.outputChannels = 2;
    state.length = length; QueueInit(initResult);
}
void ExpectFailedInit(const Info& out) {
    const auto& state = Get();
    Check(out.sampleRate==0&&out.channels==0&&out.totalFrames==0&&out.durationSeconds==0.0f,"failed init leaves default Info");
    Check(state.callCount==1&&state.calls[0]=='I',"failed init records only Init");
    Check(state.uninitCount==0,"failed init is not uninitialized");
}
#if defined(AURO_DATABASE_AUDIO_PROBE_TEST_WIN32)
void ExpectWide(const fs::path& path, const std::wstring& expected) {
    Setup(MA_ERROR); Info out{}; Check(!Detail::ProbeMiniaudio(path, out), "wide init failure");
    const auto& state=Get(); Check(state.wideCount==1&&state.wideCandidates[0]==expected,"wide candidate");
    ExpectFailedInit(out);
}
#endif
} // namespace
int main() {
    Info out{};
#if defined(AURO_DATABASE_AUDIO_PROBE_TEST_WIN32)
    ExpectWide(LR"(\\?\C:\audio.wav)",LR"(\\?\C:\audio.wav)");
    ExpectWide(LR"(\\server\share\a.wav)",LR"(\\?\UNC\server\share\a.wav)");
    ExpectWide(LR"(C:relative.wav)",LR"(\\?\C:relative.wav)");
    ExpectWide(L"relative.wav",L"relative.wav");
#elif defined(AURO_DATABASE_AUDIO_PROBE_TEST_STD)
    Setup(MA_ERROR); Check(!Detail::ProbeMiniaudio(fs::path("folder/audio.ogg"),out),"narrow init failure");
    Check(Get().narrowCount==1&&Get().narrowCandidates[0]=="folder/audio.ogg","narrow path.string");
    ExpectFailedInit(out);
#else
#error Backend test requires one private platform policy
#endif
    Setup(MA_SUCCESS); Check(Detail::ProbeMiniaudio("duration.wav",out),"successful probe");
    Check(Get().configFormat==ma_format_f32&&Get().configChannels==0&&Get().configSampleRate==0,"f32 native config");
    Check(out.sampleRate==44100&&out.channels==2&&out.totalFrames==44100&&std::fabs(out.durationSeconds-1.0f)<0.000001f,"duration and info");
    Check(Get().uninitCount==1,"one uninit");
    Check(Get().callCount==3&&Get().calls[0]=='I'&&Get().calls[1]=='L'&&Get().calls[2]=='U',"init length uninit order");
    Setup(MA_SUCCESS,22050); Get().lengthResult=MA_ERROR;
    Check(Detail::ProbeMiniaudio("length-result.wav",out),"length result is ignored");
    Check(out.sampleRate==44100&&out.channels==2&&out.totalFrames==22050&&std::fabs(out.durationSeconds-0.5f)<0.000001f,"failed length result keeps populated length");
    Check(Get().uninitCount==1&&Get().callCount==3&&Get().calls[0]=='I'&&Get().calls[1]=='L'&&Get().calls[2]=='U',"failed length result uninit order");
#if defined(AURO_DATABASE_AUDIO_PROBE_TEST_WIN32)
    Setup(MA_SUCCESS,0); QueueRead(MA_SUCCESS,5); QueueRead(MA_AT_END,3);
    Check(Detail::ProbeMiniaudio("count.wav",out)&&out.totalFrames==8,"MA_AT_END positive frames counted");
    Check(Get().requestCount==2&&Get().readRequests[0]==8192&&Get().readRequests[1]==8192,"8192 requests");
    Check(Get().callCount==5&&Get().calls[2]=='R'&&Get().calls[3]=='R'&&Get().calls[4]=='U',"count read order");
    Setup(MA_SUCCESS,0); QueueRead(MA_ERROR,4);
    Check(Detail::ProbeMiniaudio("hard.wav",out)&&out.totalFrames==4,"hard-error positive frames counted");
    Check(Get().callCount==4&&Get().calls[2]=='R'&&Get().calls[3]=='U',"hard read order");
#endif
    return failures==0?0:1;
}
