#include "PlaylistResultFilterWorkflow.h"

#include <cassert>

struct Effects final : aura::playlist::PlaylistResultFilterEffects {
    int rebuildCalls = 0;
    void rebuildFolderTree() override { ++rebuildCalls; }
};

int main() {
    Effects effects;
    aura::playlist::PlaylistResultFilterInput input;
    input.rebuildFolderTree = true;
    input.folderFilterActive = true;
    input.selectedFolderPath = "Foley/Footsteps";
    input.visibility = {true, false, true, true, true, true, true, true};
    input.records = {
        {"Foley/Footsteps", "wav"},
        {"Foley/Footsteps/Metal", "mp3"},
        {"Foley/Impacts", "wav"},
    };
    const auto kept = aura::playlist::HandlePlaylistResultFilter(input, effects);
    assert(effects.rebuildCalls == 1);
    assert((kept == std::vector<std::size_t>{0}));

    input.folderFilterActive = false;
    input.rebuildFolderTree = false;
    const auto visible = aura::playlist::HandlePlaylistResultFilter(input, effects);
    assert((visible == std::vector<std::size_t>{0, 2}));
    return 0;
}
