#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aura::same_folder {

struct SameFolderWorkflowRequest {
    std::uint64_t generation = 0;
    std::string folderPathUtf8;
    std::string missingDirectoryErrorUtf8;
};

struct SameFolderWorkflowChunk {
    std::uint64_t generation = 0;
    std::vector<std::string> filePaths;
    bool done = false;
    std::string error;
};

class SameFolderScanChunkSource {
public:
    virtual ~SameFolderScanChunkSource() = default;
    virtual bool tryPopChunk(SameFolderWorkflowChunk&) = 0;
};

class SameFolderScanEffects {
public:
    virtual ~SameFolderScanEffects() = default;
    virtual void started(const SameFolderWorkflowRequest&) = 0;
    virtual void append(const std::vector<std::string>&) = 0;
    virtual void finished(const std::string&) = 0;
};

class SameFolderScanWorkflow final {
public:
    SameFolderWorkflowRequest start(std::string folderPathUtf8,
                                    std::string missingDirectoryErrorUtf8,
                                    SameFolderScanEffects&);
    void drain(SameFolderScanChunkSource&, SameFolderScanEffects&);
    bool active() const { return active_; }
    std::uint64_t generation() const { return generation_; }

private:
    std::uint64_t generation_ = 0;
    bool active_ = false;
};

}  // namespace aura::same_folder
