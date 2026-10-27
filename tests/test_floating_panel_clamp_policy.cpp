#include "FloatingPanelLayout.h"

#include <cstdio>
#include <limits>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::printf("FAIL: %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    using aura::ui::FloatingPanelRect;
    using aura::ui::ShouldClampFloatingPanelToWorkArea;

    const FloatingPanelRect legal{-200.0f, 40.0f, 360.0f, 280.0f};
    if (!Check(!ShouldClampFloatingPanelToWorkArea(legal, false, true, false, 1920.0f, 1080.0f),
               "legal off-screen rect must stay put so multi-viewport can host it")) {
        return 1;
    }
    if (!Check(!ShouldClampFloatingPanelToWorkArea(legal, true, false, false, 1920.0f, 1080.0f),
               "dragging must never clamp")) {
        return 1;
    }

    const FloatingPanelRect nanRect{std::numeric_limits<float>::quiet_NaN(), 10.0f, 200.0f, 120.0f};
    if (!Check(ShouldClampFloatingPanelToWorkArea(nanRect, false, false, false, 1920.0f, 1080.0f),
               "NaN position must clamp")) {
        return 1;
    }

    const FloatingPanelRect zeroSize{80.0f, 80.0f, 0.0f, 120.0f};
    if (!Check(ShouldClampFloatingPanelToWorkArea(zeroSize, false, false, false, 1920.0f, 1080.0f),
               "zero-size rect must clamp")) {
        return 1;
    }

    if (!Check(ShouldClampFloatingPanelToWorkArea(legal, false, true, true, 1920.0f, 1080.0f),
               "vanished display with remaining work area must clamp back")) {
        return 1;
    }
    if (!Check(!ShouldClampFloatingPanelToWorkArea(legal, false, true, true, 0.0f, 0.0f),
               "vanished display without remaining work area must not invent a clamp target")) {
        return 1;
    }

    using aura::ui::DetectFloatingPanelDockZone;
    using aura::ui::FloatingPanelDockZone;
    const float workX = 0.0f;
    const float workY = 0.0f;
    const float workW = 1920.0f;
    const float workH = 1080.0f;
    const float snap = 80.0f;
    const float statusH = 28.0f;

    const FloatingPanelRect inLeftStrip{20.0f, 120.0f, 280.0f, 400.0f};
    if (!Check(DetectFloatingPanelDockZone(inLeftStrip, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::Left,
               "panel overlapping the inner left strip must dock left")) {
        return 1;
    }

    const FloatingPanelRect inRightStrip{1860.0f, 120.0f, 250.0f, 400.0f};
    if (!Check(DetectFloatingPanelDockZone(inRightStrip, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::Right,
               "panel overlapping the inner right strip must dock right")) {
        return 1;
    }

    const FloatingPanelRect inBottomStrip{400.0f, 990.0f, 800.0f, 180.0f};
    if (!Check(DetectFloatingPanelDockZone(inBottomStrip, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::Bottom,
               "panel overlapping the inner bottom strip must dock bottom")) {
        return 1;
    }

    const FloatingPanelRect sidebarFullyLeft{-280.0f, 120.0f, 280.0f, 400.0f};
    if (!Check(DetectFloatingPanelDockZone(sidebarFullyLeft, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::None,
               "sidebar dragged fully past the left outer edge must stay floating")) {
        return 1;
    }

    const FloatingPanelRect infoFullyRight{1920.0f, 120.0f, 250.0f, 400.0f};
    if (!Check(DetectFloatingPanelDockZone(infoFullyRight, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::None,
               "Info dragged fully past the right outer edge must stay floating")) {
        return 1;
    }

    const FloatingPanelRect secondaryMonitor{2000.0f, 40.0f, 360.0f, 280.0f};
    if (!Check(DetectFloatingPanelDockZone(secondaryMonitor, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::None,
               "panel on a secondary display must stay floating")) {
        return 1;
    }

    const FloatingPanelRect grazingLeftOuter{-280.0f, 120.0f, 280.0f, 400.0f};
    if (!Check(DetectFloatingPanelDockZone(grazingLeftOuter, workX, workY, workW, workH, snap, statusH) ==
                   FloatingPanelDockZone::None,
               "panel that only kisses the left outer edge must not dock")) {
        return 1;
    }

    using aura::ui::DisplayArea;
    using aura::ui::PanelMonitorPlacement;
    using aura::ui::ReclaimFloatingPanelIfOffDisplays;
    using aura::ui::RememberFloatingPanelMonitor;
    using aura::ui::RestoreFloatingPanelByMonitorId;

    const DisplayArea displays[] = {
        {"DISPLAY1", 0.0f, 0.0f, 1920.0f, 1080.0f, true},
        {"DISPLAY2", 2000.0f, 0.0f, 2560.0f, 1440.0f, false},
    };

    PanelMonitorPlacement savedOnSecond;
    savedOnSecond.monitorId = "DISPLAY2";
    savedOnSecond.x = 100.0f;
    savedOnSecond.y = 80.0f;
    savedOnSecond.width = 360.0f;
    savedOnSecond.height = 280.0f;
    savedOnSecond.relX = 40.0f;
    savedOnSecond.relY = 30.0f;
    savedOnSecond.hasRelative = true;
    const PanelMonitorPlacement restored = RestoreFloatingPanelByMonitorId(
        savedOnSecond, displays, 2, 160.0f, 120.0f);
    if (!Check(restored.x == 2040.0f && restored.y == 30.0f &&
                   restored.width == 360.0f && restored.height == 280.0f &&
                   restored.monitorId == "DISPLAY2" && restored.moved,
               "restart must put the panel back on the same monitor, not the old absolute position")) {
        return 1;
    }

    const DisplayArea primaryOnly[] = {
        {"DISPLAY1", 0.0f, 0.0f, 1920.0f, 1080.0f, true},
    };
    PanelMonitorPlacement unplugged = savedOnSecond;
    unplugged.x = 2200.0f;
    unplugged.y = 40.0f;
    const PanelMonitorPlacement broughtBack = RestoreFloatingPanelByMonitorId(
        unplugged, primaryOnly, 1, 160.0f, 120.0f);
    if (!Check(broughtBack.moved && broughtBack.monitorId == "DISPLAY1" &&
                   broughtBack.x >= 0.0f && broughtBack.y >= 0.0f &&
                   broughtBack.x + broughtBack.width <= 1920.0f &&
                   broughtBack.y + broughtBack.height <= 1080.0f,
               "restart after the saved monitor is gone must bring the panel fully onto the remaining screen")) {
        return 1;
    }

    PanelMonitorPlacement offScreen;
    offScreen.x = 3000.0f;
    offScreen.y = 40.0f;
    offScreen.width = 360.0f;
    offScreen.height = 280.0f;
    const PanelMonitorPlacement reclaimed = ReclaimFloatingPanelIfOffDisplays(
        offScreen, primaryOnly, 1, 160.0f, 120.0f, false);
    if (!Check(reclaimed.moved && reclaimed.x == 1560.0f && reclaimed.y == 40.0f &&
                   reclaimed.monitorId == "DISPLAY1",
               "unplugging the screen under a panel must pull it fully onto the remaining screen")) {
        return 1;
    }

    const PanelMonitorPlacement stillDragging = ReclaimFloatingPanelIfOffDisplays(
        offScreen, primaryOnly, 1, 160.0f, 120.0f, true);
    if (!Check(!stillDragging.moved && stillDragging.x == 3000.0f,
               "a panel being dragged must not be pulled back")) {
        return 1;
    }

    PanelMonitorPlacement onSecond;
    onSecond.x = 2100.0f;
    onSecond.y = 40.0f;
    onSecond.width = 360.0f;
    onSecond.height = 280.0f;
    const PanelMonitorPlacement leftAlone = ReclaimFloatingPanelIfOffDisplays(
        onSecond, displays, 2, 160.0f, 120.0f, false);
    if (!Check(!leftAlone.moved && leftAlone.x == 2100.0f && leftAlone.y == 40.0f,
               "a panel already on another screen must stay there")) {
        return 1;
    }
    const PanelMonitorPlacement remembered = RememberFloatingPanelMonitor(onSecond, displays, 2);
    if (!Check(remembered.monitorId == "DISPLAY2" && remembered.hasRelative &&
                   remembered.relX == 100.0f && remembered.relY == 40.0f && !remembered.moved,
               "the panel must remember which monitor it is on and its place on that monitor")) {
        return 1;
    }

    std::printf("OK\n");
    return 0;
}
