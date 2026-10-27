#pragma once

#include <algorithm>
#include <string>

namespace aura::ui {

// @stage7-contract
// API contract: these synchronous geometry helpers borrow only the caller's
// rectangle for the duration of the call, allocate no retained state, and may
// be called from any thread. Invalid/non-positive work areas are a no-op and
// report false; otherwise the rectangle is clamped in place and the return
// value reports whether any field changed.
// Pure geometry contract for detached panels. The work area is the usable
// viewport after taskbar/DPI insets; the result is always fully contained in
// that area while preserving the requested minimum readable size where space
// permits.
struct FloatingPanelRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

inline bool ClampFloatingPanelRect(
    FloatingPanelRect& rect,
    float workX,
    float workY,
    float workWidth,
    float workHeight,
    float minWidth,
    float minHeight) {
    if (workWidth <= 0.0f || workHeight <= 0.0f) return false;

    const FloatingPanelRect before = rect;
    const float readableMinWidth = std::min(std::max(minWidth, 1.0f), workWidth);
    const float readableMinHeight = std::min(std::max(minHeight, 1.0f), workHeight);
    rect.width = std::clamp(rect.width, readableMinWidth, workWidth);
    rect.height = std::clamp(rect.height, readableMinHeight, workHeight);
    rect.x = std::clamp(rect.x, workX, workX + workWidth - rect.width);
    rect.y = std::clamp(rect.y, workY, workY + workHeight - rect.height);
    const bool changed = rect.x != before.x || rect.y != before.y ||
        rect.width != before.width || rect.height != before.height;
    return changed;
}

// Clamp only an illegal rectangle or a vanished display that still has a
// usable remaining work area. Dragging, a valid off-screen placement, and
// startup restore of a still-legal rect must keep the requested position so
// official multi-viewport hosting can own the OS window. monitorId restore
// stays S3.
inline bool ShouldClampFloatingPanelToWorkArea(
    const FloatingPanelRect& rect,
    bool wasDragging,
    bool restoreOnStartup,
    bool displayMissing,
    float workWidth,
    float workHeight) {
    if (wasDragging) return false;
    const bool illegalSize = !(rect.width > 0.0f) || !(rect.height > 0.0f) ||
        !(rect.x == rect.x) || !(rect.y == rect.y);
    if (illegalSize) return true;
    if (displayMissing && workWidth > 0.0f && workHeight > 0.0f) return true;
    (void)restoreOnStartup;
    return false;
}

enum class FloatingPanelDockZone { None, Left, Right, Bottom };

inline bool FloatingPanelOverlapsRect(
    const FloatingPanelRect& panel,
    float x,
    float y,
    float width,
    float height) {
    return panel.width > 0.0f && panel.height > 0.0f && width > 0.0f && height > 0.0f &&
        panel.x < x + width && panel.x + panel.width > x &&
        panel.y < y + height && panel.y + panel.height > y;
}

// Docking is an inner-strip intersection, not a half-plane. A panel that has
// been dragged fully past the main window (or onto another display) must stay
// floating even if its left/right edge still satisfies the old
// `x < workX + snap` / `right > workRight - snap` tests.
inline FloatingPanelDockZone DetectFloatingPanelDockZone(
    const FloatingPanelRect& panel,
    float workX,
    float workY,
    float workWidth,
    float workHeight,
    float snapDistance,
    float reservedStatusBarHeight) {
    if (workWidth <= 0.0f || workHeight <= 0.0f || snapDistance <= 0.0f) {
        return FloatingPanelDockZone::None;
    }
    const float statusH = std::clamp(reservedStatusBarHeight, 0.0f, workHeight);
    const float usableH = workHeight - statusH;
    const float sideStrip = std::min(snapDistance, workWidth);
    const float bottomStrip = std::min(snapDistance, usableH);
    if (FloatingPanelOverlapsRect(panel, workX, workY, sideStrip, workHeight)) {
        return FloatingPanelDockZone::Left;
    }
    if (FloatingPanelOverlapsRect(panel, workX + workWidth - sideStrip, workY, sideStrip, workHeight)) {
        return FloatingPanelDockZone::Right;
    }
    if (bottomStrip > 0.0f &&
        FloatingPanelOverlapsRect(panel, workX, workY + usableH - bottomStrip, workWidth, bottomStrip)) {
        return FloatingPanelDockZone::Bottom;
    }
    return FloatingPanelDockZone::None;
}


struct DisplayArea {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    bool primary = false;
};

struct PanelMonitorPlacement {
    std::string monitorId;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float relX = 0.0f;
    float relY = 0.0f;
    bool hasRelative = false;
    bool moved = false;
};

inline bool FloatingPanelPlacementIsIllegal(const PanelMonitorPlacement& placement) {
    return !(placement.width > 0.0f) || !(placement.height > 0.0f) ||
        !(placement.x == placement.x) || !(placement.y == placement.y);
}

inline float FloatingPanelDisplayOverlapArea(
    const FloatingPanelRect& panel, const DisplayArea& display) {
    if (!(panel.width > 0.0f) || !(panel.height > 0.0f) ||
        !(display.width > 0.0f) || !(display.height > 0.0f)) {
        return 0.0f;
    }
    const float left = std::max(panel.x, display.x);
    const float top = std::max(panel.y, display.y);
    const float right = std::min(panel.x + panel.width, display.x + display.width);
    const float bottom = std::min(panel.y + panel.height, display.y + display.height);
    if (!(right > left) || !(bottom > top)) return 0.0f;
    return (right - left) * (bottom - top);
}

inline const DisplayArea* FindDisplayById(
    const std::string& id, const DisplayArea* displays, size_t displayCount) {
    if (id.empty() || displays == nullptr) return nullptr;
    for (size_t index = 0; index < displayCount; ++index) {
        if (displays[index].id == id && displays[index].width > 0.0f && displays[index].height > 0.0f) {
            return &displays[index];
        }
    }
    return nullptr;
}

inline const DisplayArea* PrimaryDisplay(const DisplayArea* displays, size_t displayCount) {
    const DisplayArea* first = nullptr;
    if (displays == nullptr) return nullptr;
    for (size_t index = 0; index < displayCount; ++index) {
        if (!(displays[index].width > 0.0f) || !(displays[index].height > 0.0f)) continue;
        if (first == nullptr) first = &displays[index];
        if (displays[index].primary) return &displays[index];
    }
    return first;
}

inline const DisplayArea* DisplayUnderPanel(
    const FloatingPanelRect& panel, const DisplayArea* displays, size_t displayCount) {
    const DisplayArea* best = nullptr;
    float bestArea = 0.0f;
    if (displays == nullptr) return nullptr;
    for (size_t index = 0; index < displayCount; ++index) {
        const float area = FloatingPanelDisplayOverlapArea(panel, displays[index]);
        if (area > bestArea) {
            bestArea = area;
            best = &displays[index];
        }
    }
    return best;
}

inline void RememberPlacementOnDisplay(PanelMonitorPlacement& placement, const DisplayArea& display) {
    placement.monitorId = display.id;
    placement.relX = placement.x - display.x;
    placement.relY = placement.y - display.y;
    placement.hasRelative = true;
}

inline PanelMonitorPlacement ClampPlacementOntoDisplay(
    PanelMonitorPlacement placement,
    const DisplayArea& display,
    float minWidth,
    float minHeight) {
    if (!(placement.width > 0.0f)) placement.width = std::max(minWidth, 1.0f);
    if (!(placement.height > 0.0f)) placement.height = std::max(minHeight, 1.0f);
    if (!(placement.x == placement.x)) placement.x = display.x;
    if (!(placement.y == placement.y)) placement.y = display.y;
    FloatingPanelRect rect{placement.x, placement.y, placement.width, placement.height};
    ClampFloatingPanelRect(rect, display.x, display.y, display.width, display.height, minWidth, minHeight);
    placement.x = rect.x;
    placement.y = rect.y;
    placement.width = rect.width;
    placement.height = rect.height;
    RememberPlacementOnDisplay(placement, display);
    return placement;
}

inline PanelMonitorPlacement RestoreFloatingPanelByMonitorId(
    const PanelMonitorPlacement& saved,
    const DisplayArea* displays,
    size_t displayCount,
    float minWidth,
    float minHeight) {
    PanelMonitorPlacement result = saved;
    result.moved = false;
    if (displays == nullptr || displayCount == 0) return result;
    const DisplayArea* matched = saved.hasRelative ? FindDisplayById(saved.monitorId, displays, displayCount) : nullptr;
    if (matched != nullptr) {
        result.x = matched->x + saved.relX;
        result.y = matched->y + saved.relY;
        result.width = saved.width;
        result.height = saved.height;
        result = ClampPlacementOntoDisplay(result, *matched, minWidth, minHeight);
    } else {
        const FloatingPanelRect rect{saved.x, saved.y, saved.width, saved.height};
        const DisplayArea* under = FloatingPanelPlacementIsIllegal(saved)
            ? nullptr
            : DisplayUnderPanel(rect, displays, displayCount);
        if (under != nullptr) {
            RememberPlacementOnDisplay(result, *under);
        } else if (const DisplayArea* primary = PrimaryDisplay(displays, displayCount)) {
            result = ClampPlacementOntoDisplay(result, *primary, minWidth, minHeight);
        }
    }
    result.moved = result.x != saved.x || result.y != saved.y ||
        result.width != saved.width || result.height != saved.height;
    return result;
}

inline PanelMonitorPlacement ReclaimFloatingPanelIfOffDisplays(
    const PanelMonitorPlacement& current,
    const DisplayArea* displays,
    size_t displayCount,
    float minWidth,
    float minHeight,
    bool wasDragging) {
    PanelMonitorPlacement result = current;
    result.moved = false;
    if (wasDragging || displays == nullptr || displayCount == 0) return result;
    const FloatingPanelRect rect{current.x, current.y, current.width, current.height};
    const DisplayArea* under = FloatingPanelPlacementIsIllegal(current)
        ? nullptr
        : DisplayUnderPanel(rect, displays, displayCount);
    if (under != nullptr) {
        RememberPlacementOnDisplay(result, *under);
        return result;
    }
    if (const DisplayArea* primary = PrimaryDisplay(displays, displayCount)) {
        result = ClampPlacementOntoDisplay(result, *primary, minWidth, minHeight);
        result.moved = result.x != current.x || result.y != current.y ||
            result.width != current.width || result.height != current.height;
    }
    return result;
}

inline PanelMonitorPlacement RememberFloatingPanelMonitor(
    const PanelMonitorPlacement& current,
    const DisplayArea* displays,
    size_t displayCount) {
    PanelMonitorPlacement result = current;
    result.moved = false;
    if (displays == nullptr || displayCount == 0 || FloatingPanelPlacementIsIllegal(current)) return result;
    const FloatingPanelRect rect{current.x, current.y, current.width, current.height};
    const DisplayArea* under = DisplayUnderPanel(rect, displays, displayCount);
    if (under == nullptr) return result;
    RememberPlacementOnDisplay(result, *under);
    return result;
}


} // namespace aura::ui
