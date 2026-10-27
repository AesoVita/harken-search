#include "src/DragPauseOnStartPolicy.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++g_failures;
    } else {
        std::cout << "PASS: " << message << "\n";
    }
}

}  // namespace

int main() {
    using aura::drag_pause::DecidePauseOnDragStart;

    const auto offPlaying = DecidePauseOnDragStart(false, true, true);
    Check(!offPlaying.shouldPause, "preference off does not pause a real drag");
    Check(std::string(offPlaying.reason) == "preference_off",
          "preference off reason is preference_off");

    const auto onIdle = DecidePauseOnDragStart(true, false, true);
    Check(!onIdle.shouldPause, "no playing audio does not invent a pause");
    Check(std::string(onIdle.reason) == "not_playing", "idle reason is not_playing");

    const auto hover = DecidePauseOnDragStart(true, true, false);
    Check(!hover.shouldPause, "hover/click/selection is not an in-scope drag start");
    Check(std::string(hover.reason) == "not_in_scope_drag",
          "out-of-scope reason is not_in_scope_drag");

    const auto onPlaying = DecidePauseOnDragStart(true, true, true);
    Check(onPlaying.shouldPause, "enabled + playing + drag start pauses immediately");
    Check(std::string(onPlaying.reason) == "pause_on_drag_start",
          "pause reason is pause_on_drag_start");

    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
