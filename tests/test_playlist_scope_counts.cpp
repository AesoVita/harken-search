#include "PlaylistScopeCounts.h"

#include <cassert>

int main() {
    const auto counts = aura::playlist::ResolvePlaylistVisibleCounts(4, 2);
    assert(counts.matchCount == 4);
    assert(counts.loadedCount == 2);
    assert(aura::playlist::ResolvePlaylistResultTitleCount(4, 2) == 4);
    assert(aura::playlist::ResolvePlaylistResultTitleCount(0, 0) == 0);
    return 0;
}
