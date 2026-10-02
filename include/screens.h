#pragma once

#include "placement.h"

#include <string>
#include <utility>
#include <vector>

// Screen geometry helpers (Qt-based; kept out of Utils so the unit tests
// stay Qt-free and runnable without a display server).
//
// Both entry points must be called on the GUI thread - QScreen is not
// thread-safe. Every caller already is: the Gaps tab, crash recovery (which
// prompts on the same thread) and ConkyManager, which deliberately routes
// panel launches through QTimer::singleShot rather than a std::thread.
class Screens {
public:
    // Every output with its desktop-space geometry, in the compositor's
    // advertised order. Empty when no GUI is running.
    static std::vector<MonitorRect> monitors();

    // Top-left of the NAMED output, in desktop coordinates:
    //   X11      one global space, so always (0,0) - there is no origin to
    //            subtract and matching a name would be actively wrong
    //   Wayland  the origin that panel's layer-shell margins are measured
    //            from, which is what desktop <-> file conversion applies
    // An empty or unknown name falls back to the primary screen, then the
    // first screen, then (0,0) - mirroring conky's own output fallback.
    //
    // Deliberately takes the output name rather than reading the ambient
    // CONKY_WAYLAND_OUTPUT: two panels on two monitors cannot both be
    // resolved from one process-wide variable.
    static std::pair<int, int> output_origin(const std::string& name);
};
