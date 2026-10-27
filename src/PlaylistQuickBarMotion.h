#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

// @stage7-contract
// API contract: playlist quick-bar helpers are synchronous UI value/policy
// operations. Container templates mutate only caller-owned collections;
// rename buffering borrows the output array for the call and always terminates
// it when valid. No ImGui state, callbacks, or references are retained.

inline int PlaylistQuickBarSwapDirection(
    int draggedIndex,
    float draggedCenterX,
    float previousCenterX,
    float nextCenterX,
    int itemCount)
{
    if (draggedIndex < 0 || draggedIndex >= itemCount) {
        return 0;
    }
    if (draggedIndex > 0 && draggedCenterX < previousCenterX) {
        return -1;
    }
    if (draggedIndex < itemCount - 1 && draggedCenterX > nextCenterX) {
        return 1;
    }
    return 0;
}

template <typename Container>
inline bool PlaylistQuickBarSwapItems(Container& items, int indexA, int indexB)
{
    const int count = static_cast<int>(items.size());
    if (indexA < 0 || indexB < 0 || indexA >= count || indexB >= count || indexA == indexB) {
        return false;
    }

    using std::swap;
    swap(items[(size_t)indexA], items[(size_t)indexB]);
    return true;
}

template <typename Container>
inline bool PlaylistQuickBarContainsId(const Container& ids, int64_t playlistId)
{
    return std::find(ids.begin(), ids.end(), playlistId) != ids.end();
}

template <typename Container>
inline bool PlaylistQuickBarIsPlaylistVisible(const Container& hiddenIds, int64_t playlistId)
{
    return playlistId > 0 && !PlaylistQuickBarContainsId(hiddenIds, playlistId);
}

template <typename Container>
inline bool PlaylistQuickBarSetPlaylistVisible(Container& hiddenIds, int64_t playlistId, bool visible)
{
    if (playlistId <= 0) {
        return false;
    }

    auto it = std::find(hiddenIds.begin(), hiddenIds.end(), playlistId);
    if (visible) {
        if (it == hiddenIds.end()) {
            return false;
        }
        hiddenIds.erase(it);
        return true;
    }

    if (it != hiddenIds.end()) {
        return false;
    }
    hiddenIds.push_back(playlistId);
    return true;
}

inline void PlaylistQuickBarSpringStep(
    float& currentX,
    float& velocityX,
    float targetX,
    float deltaSeconds)
{
    const float dt = std::clamp(deltaSeconds, 0.0f, 1.0f / 30.0f);
    if (dt <= 0.0f) {
        return;
    }

    if (std::fabs(targetX - currentX) < 0.08f && std::fabs(velocityX) < 0.08f) {
        currentX = targetX;
        velocityX = 0.0f;
        return;
    }

    constexpr float kStiffness = 520.0f;
    constexpr float kDamping = 34.0f;
    const float acceleration = (targetX - currentX) * kStiffness - velocityX * kDamping;
    velocityX += acceleration * dt;
    currentX += velocityX * dt;

    if (std::fabs(targetX - currentX) < 0.08f && std::fabs(velocityX) < 0.08f) {
        currentX = targetX;
        velocityX = 0.0f;
    }
}

inline bool PlaylistRenamePrepareBuffer(
    int64_t playlistId,
    const std::string& currentName,
    char* outBuffer,
    size_t outBufferSize)
{
    if (playlistId <= 0 || outBuffer == nullptr || outBufferSize == 0) {
        return false;
    }

    std::snprintf(outBuffer, outBufferSize, "%s", currentName.c_str());
    outBuffer[outBufferSize - 1] = '\0';
    return true;
}

inline bool PlaylistRenameCandidateIsDuplicate(
    int64_t editingPlaylistId,
    int64_t otherPlaylistId,
    const std::string& candidateName,
    const std::string& otherName)
{
    return otherPlaylistId != editingPlaylistId &&
           !candidateName.empty() &&
           candidateName == otherName;
}

inline const char* PlaylistRenamePopupStableId()
{
    return "PlaylistRenamePopup";
}

inline std::string PlaylistRenamePopupLabel(const char* visibleTitle)
{
    std::string label = visibleTitle != nullptr ? visibleTitle : "";
    label += "###";
    label += PlaylistRenamePopupStableId();
    return label;
}
