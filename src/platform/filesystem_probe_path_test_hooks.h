#pragma once

#ifndef AURO_FILESYSTEM_PROBE_PATH_TESTING
#error "ProbePath hooks are test-only"
#endif

#include <cstdint>
#include <string>

namespace Platform::FileSystem::Testing {

struct ProbePathScript {
    uint32_t attributes = 0xFFFFFFFF;  // INVALID_FILE_ATTRIBUTES
    uint32_t lastError = 0;
    int calls = 0;
    int delayMs = 0;
};

struct DriveTypeScript {
    bool overrideDriveType = false;
    uint32_t driveType = 0;
    int calls = 0;
};

struct NetworkPathForceScript {
    bool forceAll = false;
    std::string prefix;
    int calls = 0;
};

void SetProbePathScript(ProbePathScript* script);
void SetDriveTypeScript(DriveTypeScript* script);
void SetNetworkPathForceScript(NetworkPathForceScript* script);

} // namespace Platform::FileSystem::Testing
