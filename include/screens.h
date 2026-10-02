#pragma once

#include <utility>

// Screen geometry helpers (Qt-based; kept out of Utils so the unit tests
// stay Qt-free and runnable without a display server).
class Screens {
public:
    // Top-left of the output the active server positions panels on, in
    // desktop coordinates. X11 uses one global space (always 0,0, i.e. no
    // conversion). Wayland layer-shell margins are output-relative, so this
    // returns the target output's origin, matched by CONKY_WAYLAND_OUTPUT
    // (falling back to the primary screen, then 0,0). The Gaps tab shows
    // desktop coordinates and adds/subtracts this origin on load/save.
    static std::pair<int, int> active_output_origin();
};
