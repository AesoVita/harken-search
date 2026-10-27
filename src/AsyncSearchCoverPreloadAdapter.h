#pragma once

#include "AsyncSearchCoverPreloadWorkflow.h"

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>

namespace aura::search {

struct AsyncSearchCoverPreloadRecord {
    std::string folderPath;
    std::string embeddedCoverKey;
    std::string filePath;
};

class AsyncSearchCoverPreloadAdapter final
    : public AsyncSearchCoverPreloadEffects {
public:
    using RecordProvider = std::function<bool(
        std::size_t,
        AsyncSearchCoverPreloadRecord&)>;
    using FolderPreloader = std::function<void(const std::string&)>;
    using EmbeddedPreloader = std::function<void(
        const std::string&,
        const std::string&)>;

    AsyncSearchCoverPreloadAdapter(
        RecordProvider records,
        FolderPreloader preloadFolder,
        EmbeddedPreloader preloadEmbedded);

    void preloadResult(std::size_t index) override;

private:
    RecordProvider records_;
    FolderPreloader preloadFolder_;
    EmbeddedPreloader preloadEmbedded_;
    std::unordered_set<std::string> folders_;
};

}  // namespace aura::search
