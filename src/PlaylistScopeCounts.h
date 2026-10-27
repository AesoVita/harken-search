#pragma once

#include <cstdint>

namespace aura::playlist {

// Visible playlist counts stay on the complete filtered match set.
// A loaded page is only what the list has drawn so far.
struct PlaylistVisibleCounts {
    int64_t matchCount = 0;
    int64_t loadedCount = 0;
};

inline PlaylistVisibleCounts ResolvePlaylistVisibleCounts(int64_t matchCount,
                                                          int64_t loadedCount) {
    PlaylistVisibleCounts counts;
    counts.matchCount = matchCount < 0 ? int64_t{0} : matchCount;
    counts.loadedCount = loadedCount < 0 ? int64_t{0} : loadedCount;
    return counts;
}

inline int64_t ResolvePlaylistResultTitleCount(int64_t matchCount,
                                               int64_t loadedPageCount) {
    (void)loadedPageCount;
    return ResolvePlaylistVisibleCounts(matchCount, loadedPageCount).matchCount;
}

}  // namespace aura::playlist
