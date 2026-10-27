#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "src/PlaylistQuickBarMotion.h"

int main()
{
    int failed = 0;

    if (PlaylistQuickBarSwapDirection(2, 145.0f, 160.0f, 260.0f, 5) != -1)
        failed++;
    if (PlaylistQuickBarSwapDirection(2, 280.0f, 160.0f, 260.0f, 5) != 1)
        failed++;
    if (PlaylistQuickBarSwapDirection(0, 20.0f, 0.0f, 120.0f, 5) != 0)
        failed++;
    if (PlaylistQuickBarSwapDirection(4, 500.0f, 420.0f, 0.0f, 5) != 0)
        failed++;

    {
        std::vector<int64_t> order = {1, 2, 3, 4};
        if (!PlaylistQuickBarSwapItems(order, 1, 2))
            failed++;
        const std::vector<int64_t> expected = {1, 3, 2, 4};
        if (order != expected)
            failed++;
        if (PlaylistQuickBarSwapItems(order, -1, 0))
            failed++;
        if (PlaylistQuickBarSwapItems(order, 1, 1))
            failed++;
        if (PlaylistQuickBarSwapItems(order, 1, 9))
            failed++;
    }

    {
        float current = 0.0f;
        float velocity = 0.0f;
        PlaylistQuickBarSpringStep(current, velocity, 100.0f, 1.0f / 60.0f);
        if (!(current > 0.0f && current < 100.0f))
            failed++;
        if (!(velocity > 0.0f))
            failed++;
    }

    {
        float current = 99.96f;
        float velocity = 0.02f;
        PlaylistQuickBarSpringStep(current, velocity, 100.0f, 1.0f / 60.0f);
        if (std::fabs(current - 100.0f) > 0.0001f || std::fabs(velocity) > 0.0001f)
            failed++;
    }

    {
        char buffer[16] = {};
        if (!PlaylistRenamePrepareBuffer(42, "magic", buffer, sizeof(buffer)))
            failed++;
        if (std::strcmp(buffer, "magic") != 0)
            failed++;
    }

    {
        char buffer[5] = {};
        if (!PlaylistRenamePrepareBuffer(42, "whoosh", buffer, sizeof(buffer)))
            failed++;
        if (std::strcmp(buffer, "whoo") != 0)
            failed++;
    }

    {
        char buffer[16] = "keep";
        if (PlaylistRenamePrepareBuffer(0, "bad", buffer, sizeof(buffer)))
            failed++;
        if (std::strcmp(buffer, "keep") != 0)
            failed++;
    }

    if (PlaylistRenameCandidateIsDuplicate(42, 42, "magic", "magic"))
        failed++;
    if (!PlaylistRenameCandidateIsDuplicate(42, 7, "magic", "magic"))
        failed++;

    {
        std::vector<int64_t> hiddenIds;
        if (!PlaylistQuickBarIsPlaylistVisible(hiddenIds, 7))
            failed++;
        if (!PlaylistQuickBarSetPlaylistVisible(hiddenIds, 7, false))
            failed++;
        if (PlaylistQuickBarIsPlaylistVisible(hiddenIds, 7))
            failed++;
        if (PlaylistQuickBarSetPlaylistVisible(hiddenIds, 7, false))
            failed++;
        if (!PlaylistQuickBarSetPlaylistVisible(hiddenIds, 7, true))
            failed++;
        if (!PlaylistQuickBarIsPlaylistVisible(hiddenIds, 7) || !hiddenIds.empty())
            failed++;
        if (PlaylistQuickBarSetPlaylistVisible(hiddenIds, 0, false))
            failed++;
    }

    {
        const std::string label = PlaylistRenamePopupLabel("Rename Playlist");
        if (label != "Rename Playlist###PlaylistRenamePopup")
            failed++;
    }

    if (failed) {
        std::printf("FAIL: %d assertion(s)\n", failed);
        return 1;
    }

    std::printf("PASS\n");
    return 0;
}
