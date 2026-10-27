#pragma once

#include <string>

namespace aura::audio_export {

struct SingleChannelExportRequest {
    std::string sourcePath;
    std::string sourceFileName;
    std::string destinationDirectory;
    int channelIndex = 0;
    int totalChannels = 0;
};

struct SingleChannelExportResult {
    bool ok = false;
    std::string outputPath;
    std::string errorMessage;
};

SingleChannelExportResult RunSingleChannelExport(
    SingleChannelExportRequest request);

}  // namespace aura::audio_export
