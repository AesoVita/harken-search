#include "PlaylistCoverScope.h"

#include <cassert>
#include <string>

namespace {

AudioRecord RecordWithFolderCover(int64_t id, const std::string& folder, const std::string& key) {
    AudioRecord record;
    record.id = id;
    record.folderPath = folder;
    record.folderCoverKey = key;
    record.hasCover = true;
    return record;
}

const aura::playlist::PlaylistCoverGroup* FindGroup(
    const std::vector<aura::playlist::PlaylistCoverGroup>& groups,
    const std::string& key) {
    for (const auto& group : groups) {
        if (group.key == key) return &group;
    }
    return nullptr;
}

}  // namespace

int main() {
    const std::vector<AudioRecord> completeMembers = {
        RecordWithFolderCover(1, "H:/FX/Fire", "F|PATH|H:/FX/Fire"),
        RecordWithFolderCover(2, "H:/FX/Fire", "F|PATH|H:/FX/Fire"),
        RecordWithFolderCover(3, "H:/FX/Whoosh", "F|PATH|H:/FX/Whoosh"),
    };
    const std::vector<AudioRecord> loadedPage = {completeMembers[0]};

    const auto groups = aura::playlist::BuildPlaylistCoverGroupsFromCompleteMembers(
        completeMembers, loadedPage);
    const auto* whoosh = FindGroup(groups, "F|PATH|H:/FX/Whoosh");
    const auto* fire = FindGroup(groups, "F|PATH|H:/FX/Fire");
    assert(whoosh != nullptr);
    assert(fire != nullptr && fire->hitCount == 2);
    return 0;
}
