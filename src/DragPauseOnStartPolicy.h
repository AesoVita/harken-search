#pragma once

namespace aura::drag_pause {

struct DragPauseDecision {
    bool shouldPause = false;
    const char* reason = "none";
};

// Pure contract for RQ-20260819-01:
// pause only when the preference is on, an in-scope drag has actually started,
// and audio is currently playing. Hover, click, selection, and marquee
// selection are out of scope. This decision does not restart playback.
inline DragPauseDecision DecidePauseOnDragStart(
    bool preferenceEnabled,
    bool audioIsPlaying,
    bool inScopeDragStarted)
{
    if (!inScopeDragStarted) {
        return {false, "not_in_scope_drag"};
    }
    if (!preferenceEnabled) {
        return {false, "preference_off"};
    }
    if (!audioIsPlaying) {
        return {false, "not_playing"};
    }
    return {true, "pause_on_drag_start"};
}

}  // namespace aura::drag_pause
