#include "AsyncSearchPlaylistContextWorkflow.h"

#include <cassert>
#include <string>
#include <vector>

struct Effects final : aura::search::AsyncSearchPlaylistContextEffects {
    std::vector<std::string> calls;
    void applyPlaylistContext(std::int64_t, bool, bool, const std::string&) override {
        calls.push_back("apply-playlist");
    }
    void reportPlaylistContext(const aura::search::AsyncSearchPlaylistContextInput&) override {
        calls.push_back("report-playlist");
    }
    void applyLibraryContext() override { calls.push_back("apply-library"); }
    void reportLibraryContext(const aura::search::AsyncSearchPlaylistContextInput&) override {
        calls.push_back("report-library");
    }
};

int main() {
    Effects effects;
    aura::search::AsyncSearchPlaylistContextInput input;
    input.kind = aura::search::AsyncSearchPlaylistContextKind::Playlist;
    input.playlistId = 42;
    aura::search::HandleAsyncSearchPlaylistContext(input, effects);
    assert((effects.calls == std::vector<std::string>{"apply-playlist", "report-playlist"}));

    effects.calls.clear();
    input.kind = aura::search::AsyncSearchPlaylistContextKind::Library;
    aura::search::HandleAsyncSearchPlaylistContext(input, effects);
    assert((effects.calls == std::vector<std::string>{"apply-library", "report-library"}));

    effects.calls.clear();
    input.kind = aura::search::AsyncSearchPlaylistContextKind::None;
    aura::search::HandleAsyncSearchPlaylistContext(input, effects);
    assert(effects.calls.empty());
    return 0;
}
