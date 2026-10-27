#pragma once

#include "platform/audio_file.h"
#include <string>
#include <vector>

namespace AudioFileScripted {
using FrameCount = Platform::AudioFile::FrameCount;
using LegacyWindowsPathPolicy = Platform::AudioFile::LegacyWindowsPathPolicy;
using MonoSampleRate = Platform::AudioFile::MonoSampleRate;
using ReadResult = Platform::AudioFile::ReadResult;
using Status = Platform::AudioFile::Status;
struct ReadStep { ReadResult result; std::vector<float> payload; };
struct State {
    std::vector<Status> openQueue, seekQueue;
    std::vector<ReadStep> readQueue;
    std::vector<std::string> calls, paths;
    std::vector<LegacyWindowsPathPolicy> policies;
    std::vector<MonoSampleRate> sampleRates;
    std::vector<FrameCount> seeks, readRequests;
    std::size_t openIndex = 0, seekIndex = 0, readIndex = 0;
};
State& Get();
void Reset();
void QueueOpen(Status status);
void QueueSeek(Status status);
void QueueRead(ReadResult result, std::vector<float> payload = {});
} // namespace AudioFileScripted
