#include "database_audio_probe_scripted_backend.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <vector>

namespace fs = std::filesystem;
using namespace Platform::DatabaseAudioProbe;
using namespace DatabaseAudioProbeScripted;
namespace {
int failures = 0;
void Check(bool value, const char* message) { if (!value) { std::printf("FAIL: %s\n", message); ++failures; } }
void U16(std::vector<unsigned char>& b, uint16_t v) { b.push_back(v & 255); b.push_back(v >> 8); }
void U32(std::vector<unsigned char>& b, uint32_t v) { for (int i=0;i<4;++i) b.push_back((v >> (8*i)) & 255); }
void U64(std::vector<unsigned char>& b, uint64_t v) { for (int i=0;i<8;++i) b.push_back((v >> (8*i)) & 255); }
void Tag(std::vector<unsigned char>& b, const char* s) { b.insert(b.end(), s, s + 4); }
void WriteWave(const fs::path& path, bool rf64) {
    std::vector<unsigned char> b; Tag(b, rf64 ? "RF64" : "RIFF"); U32(b, rf64 ? 0xffffffffu : 0); Tag(b, "WAVE");
    if (rf64) { Tag(b,"ds64"); U32(b,28); U64(b,80); U64(b,8); U64(b,4); U32(b,0); }
    Tag(b,"fmt "); U32(b,16); U16(b,1); U16(b,1); U32(b,22050); U32(b,44100); U16(b,2); U16(b,16);
    Tag(b,"data"); U32(b,rf64 ? 0xffffffffu : 8); for(int i=0;i<8;++i)b.push_back(0);
    if (!rf64) { const uint32_t n=static_cast<uint32_t>(b.size()-8); for(int i=0;i<4;++i)b[4+i]=(n>>(8*i))&255; }
    std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(b.data()), b.size());
}
bool Same(const Info& a, const Info& b) {
    return a.sampleRate==b.sampleRate && a.channels==b.channels && a.totalFrames==b.totalFrames &&
           std::fabs(a.durationSeconds-b.durationSeconds)<0.000001f;
}
void Expect(std::initializer_list<Call> calls, const std::string& path) {
    const auto got=Get(); Check(got.calls==std::vector<Call>(calls), "exact call order");
    Check(got.paths.size()==calls.size(), "path count");
    for(const auto& value:got.paths) Check(value==path, "exact UTF-8 path");
}
Step S(bool result, Info info={}, std::string error={}) { return {result,info,std::move(error)}; }
} // namespace

int main() {
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path base=fs::temp_directory_path()/fs::path("aris_probe_routing_"+std::to_string(stamp));
    const fs::path riff=base.string()+".wav", rf64=base.string()+".wave", malformed=base.string()+"_bad.wav";
    WriteWave(riff,false); WriteWave(rf64,true); { std::ofstream out(malformed); out << "bad"; }
    for(const auto& path:{riff,rf64}) { Reset(); Info out{9,9,9,9}; Check(Probe(path,out),"WAV fast success");
        Check(Same(out,{22050,1,4,4.0f/22050.0f}),"WAV exact info"); Check(Get().calls.empty(),"WAV bypasses backends"); }

#if defined(AURO_DATABASE_AUDIO_PROBE_TEST_WIN32)
    const std::string wma=u8"routing 音效.wma"; const auto wmaPath=fs::u8path(wma);
    Reset(); QueueIsFlac(false); QueueCanDecode(true); QueueMediaFoundation(S(true,{48000,2,96000,2.0f}));
    Info out{}; Check(Probe(wmaPath,out)&&Same(out,{48000,2,96000,2.0f}),"MF success");
    Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::MediaFoundation},wma);
    Reset(); QueueIsFlac(false); QueueCanDecode(true); QueueMediaFoundation(S(false,{},"codec unavailable"));
    Check(!Probe(wmaPath,out),"MF failure"); Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::MediaFoundation},wma);
    const std::string flac="routing.flac";
    Reset(); QueueIsFlac(true); QueueCanDecode(true); QueueMiniaudio(S(true,{44100,2,88200,2.0f}));
    Check(Probe(flac,out)&&Same(out,{44100,2,88200,2.0f}),"FLAC miniaudio success"); Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::Miniaudio},flac);
    Reset(); QueueIsFlac(true); QueueCanDecode(true); QueueMiniaudio(S(false)); QueueMediaFoundation(S(true,{32000,1,16000,0.5f}));
    Check(Probe(flac,out)&&Same(out,{32000,1,16000,0.5f}),"FLAC MF fallback success");
    Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::Miniaudio,Call::MediaFoundation},flac);
    Reset(); QueueIsFlac(true); QueueCanDecode(true); QueueMiniaudio(S(false)); QueueMediaFoundation(S(false,{},"flac codec unavailable"));
    Check(!Probe(flac,out),"FLAC fallback failure");
    Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::Miniaudio,Call::MediaFoundation},flac);
    for(const auto& path:{malformed,fs::path("routing.ogg")}) { const std::string p=path.string(); Reset(); QueueIsFlac(false); QueueCanDecode(false);
        QueueMiniaudio(S(true,{22050,1,11,11.0f/22050.0f})); Check(Probe(path,out)&&Same(out,{22050,1,11,11.0f/22050.0f}),"miniaudio route");
        Expect({Call::IsFlacExtension,Call::CanDecodeExtension,Call::Miniaudio},p); }
#elif defined(AURO_DATABASE_AUDIO_PROBE_TEST_STD)
    Info out{}; for(const auto& path:{malformed,fs::path("routing.ogg")}) { Reset(); QueueMiniaudio(S(true,{22050,1,11,11.0f/22050.0f}));
        Check(Probe(path,out)&&Same(out,{22050,1,11,11.0f/22050.0f}),"Std miniaudio route"); Expect({Call::Miniaudio},path.string()); }
#else
#error Routing test requires one private platform policy
#endif
    std::error_code ec; fs::remove(riff,ec); fs::remove(rf64,ec); fs::remove(malformed,ec);
    return failures==0 ? 0 : 1;
}
