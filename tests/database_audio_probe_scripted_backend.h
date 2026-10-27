#pragma once

#include "platform/database_audio_probe.h"

#include <cstddef>
#include <string>
#include <vector>

namespace DatabaseAudioProbeScripted {

using Info = Platform::DatabaseAudioProbe::Info;
constexpr std::size_t kCapacity = 32;

enum class Call { CanDecodeExtension, IsFlacExtension, MediaFoundation, Miniaudio };
struct Step { bool result = false; Info info{}; std::string error; };
struct Snapshot {
    std::vector<Call> calls;
    std::vector<std::string> paths;
};

void Reset();
bool QueueCanDecode(bool result);
bool QueueIsFlac(bool result);
bool QueueMediaFoundation(const Step& step);
bool QueueMiniaudio(const Step& step);
Snapshot Get();

} // namespace DatabaseAudioProbeScripted
