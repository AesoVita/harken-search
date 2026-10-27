#include "platform/database_audio_probe.h"

int main() {
    Platform::DatabaseAudioProbe::Info info;
    return info.sampleRate == 0 && info.channels == 0 &&
                   info.totalFrames == 0 && info.durationSeconds == 0.0f
        ? 0
        : 1;
}
