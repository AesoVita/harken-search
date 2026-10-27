#pragma once

#include <cstdint>
#include <string>

// Pure-data send-to-DAW spot contract. It mirrors the raw log_spotting
// log_spotting payload without depending on Database/AppState hot paths.
// @stage7-contract
// API contract: SpotRequest is a value-owned transport description; strings
// and scalar fields are owned by the instance and no database/audio resources
// or lifetimes are implied. Consumers validate ranges and destination/host
// capability before dispatch while preserving the existing payload layout.
struct SpotRequest {
    std::string daw;
    bool spotAsRegion = false;
    std::string destinationFilePath;
    std::string regionName;
    uint64_t totalFrames = 0;
    uint32_t sampleRate = 0;
    uint64_t handleFrameIn = 0;
    uint64_t handleFramesOut = 0;
    uint64_t startSample = 0;
    uint64_t stopSample = 0;
    uint64_t spotInTime = 0;
    int spotOntoTrack = 0;
    int spotIntoStreams = 0;
};
