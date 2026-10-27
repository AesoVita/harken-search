#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aura::same_folder {

struct SameFolderScanRequest {
    uint64_t generation = 0;
    std::string folderPathUtf8;
    std::string missingDirectoryErrorUtf8;
};

struct SameFolderScanChunk {
    uint64_t generation = 0;
    std::vector<std::string> filePaths;
    bool done = false;
    std::string error;
};

class SameFolderScanRuntime final {
public:
    SameFolderScanRuntime();
    ~SameFolderScanRuntime();

    SameFolderScanRuntime(const SameFolderScanRuntime&) = delete;
    SameFolderScanRuntime& operator=(const SameFolderScanRuntime&) = delete;

    void submitLatest(const SameFolderScanRequest& request);
    bool tryPopChunk(SameFolderScanChunk& chunk);
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aura::same_folder
