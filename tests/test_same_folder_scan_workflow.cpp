#include "src/SameFolderScanWorkflow.h"

#include <cassert>
#include <vector>

using namespace aura::same_folder;

struct Source final : SameFolderScanChunkSource {
    std::vector<SameFolderWorkflowChunk> chunks;
    bool tryPopChunk(SameFolderWorkflowChunk& chunk) override {
        if (chunks.empty()) return false;
        chunk = std::move(chunks.front());
        chunks.erase(chunks.begin());
        return true;
    }
};

struct Effects final : SameFolderScanEffects {
    SameFolderWorkflowRequest request;
    std::vector<std::string> paths;
    std::string error;
    void started(const SameFolderWorkflowRequest& value) override { request = value; }
    void append(const std::vector<std::string>& value) override {
        paths.insert(paths.end(), value.begin(), value.end());
    }
    void finished(const std::string& value) override { error = value; }
};

int main() {
    SameFolderScanWorkflow workflow;
    Effects effects;
    const auto request = workflow.start("C:/audio", "missing", effects);
    assert(request.generation == 1);
    Source source;
    source.chunks.push_back({0, {"stale"}, false, {}});
    source.chunks.push_back({1, {"a", "b"}, false, {}});
    source.chunks.push_back({1, {"c"}, true, "done"});
    workflow.drain(source, effects);
    assert(effects.paths == std::vector<std::string>({"a", "b", "c"}));
    assert(effects.error == "done");
    assert(!workflow.active());
    return 0;
}
