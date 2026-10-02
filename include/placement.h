#pragma once

#include <string>
#include <utility>
#include <vector>

// Per-panel placement: pure geometry and the record the Placement tab edits.
//
// Deliberately free of Qt, like the rest of the Qt-free core. Monitor maths
// and the desktop<->output-relative conversion must be unit-testable without
// a display server; screens.h is the Qt-facing side of that boundary.
//
// Coordinate system
// -----------------
// Everything stored in the config is in DESKTOP coordinates - the global
// space every monitor is laid out in - whatever the display server does with
// it downstream:
//
//   X11      conky positions windows absolutely, so gap_x/gap_y take the
//            desktop coordinate unchanged.
//   Wayland  panels mount through wlr-layer-shell, whose margins are measured
//            inside a single output, so gap_x/gap_y become
//            (desktop coordinate - output origin).
//
// Keeping one meaning in the config is what lets a drop on the placement map
// be stored directly, with conversion deferred to write-out.

// A monitor as it sits in the desktop's coordinate space.
struct MonitorRect {
    std::string name;   // Output name as the compositor advertises it ("HDMI-A-1")
    int x = 0;          // Top-left corner, desktop coordinates
    int y = 0;
    int w = 0;
    int h = 0;
    bool primary = false;
};

// Where a panel lives. See the coordinate note above for x/y.
struct PanelPlacement {
    std::string output;      // Monitor name; "" = let the compositor choose
    int x = 0;               // Desktop coordinates
    int y = 0;
    bool launch_env = true;  // Inject CONKY_WAYLAND_OUTPUT when UCCC launches it
};

// A desktop point resolved to a monitor plus the output-relative offset
// inside it.
struct ResolvedPoint {
    const MonitorRect* monitor = nullptr;  // nullptr only when the list is empty
    int rel_x = 0;
    int rel_y = 0;
};

// Axis-aligned box spanning a set of monitors.
struct DesktopBox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// ── Monitor lookup ───────────────────────────────────────────────────────────

// Monitor containing (px, py), or nullptr when the point is outside all of
// them. Overlaps resolve deterministically: the smallest containing monitor
// wins, then the earliest entry, so scan order cannot change the answer.
const MonitorRect* monitor_for_point(const std::vector<MonitorRect>& monitors,
                                     int px, int py);

// Monitor whose centre is closest, by squared distance. For points that fall
// outside every monitor - a gap between them, or past a desktop edge.
const MonitorRect* nearest_monitor(const std::vector<MonitorRect>& monitors,
                                   int px, int py);

// Monitor with an exact name match, or nullptr.
const MonitorRect* monitor_by_name(const std::vector<MonitorRect>& monitors,
                                   const std::string& name);

// The monitor flagged primary, or nullptr when none is. conky falls back to
// the first output it hears about, which in practice is the primary.
const MonitorRect* primary_monitor(const std::vector<MonitorRect>& monitors);

// Resolve a desktop point to monitor + output-relative offset, falling back
// to the nearest monitor so a drag can never yield an unassigned panel.
ResolvedPoint resolve_output_point(const std::vector<MonitorRect>& monitors,
                                   int px, int py);

// Bounding box spanning every monitor. Monitors left of or above the origin
// produce negative x/y, which the map widget must handle.
DesktopBox desktop_bounds(const std::vector<MonitorRect>& monitors);

// ── Coordinate conversion ────────────────────────────────────────────────────

// Desktop -> output-relative, and back. Layer-shell accepts negative margins,
// so an offset that leaves the monitor is meaningful and is NOT clamped.
std::pair<int, int> to_relative(const MonitorRect& m, int x, int y);
std::pair<int, int> to_desktop(const MonitorRect& m, int rel_x, int rel_y);

// ── Import ───────────────────────────────────────────────────────────────────

// Build a desktop-coordinate placement from what a conky .conf already says,
// so panels positioned before UCCC tracked placement appear in the map instead
// of showing up unplaced. Never writes anything - it only lifts the conf's
// numbers into desktop coordinates.
//
//   gap_x/gap_y      as read from the conf
//   conf_output      the conf's wayland_output, or "" when absent
//   wayland          true when the active display server is Wayland, i.e. the
//                    gap values are output-relative margins rather than
//                    desktop coordinates
//   monitors         the current layout, used to turn margins into desktop
//                    coordinates; may be empty
//   fallback_output  ambient output used when the conf names none, or names
//                    one that is no longer connected ("" = leave unassigned)
//
// Output resolution order: conf_output, then fallback_output, then primary,
// then the first monitor - mirroring conky's own fallback chain. A name that
// is not in `monitors` is replaced by whichever monitor was chosen, so the
// stored record never points at an unplugged output. With no monitors at all
// the gaps are taken at face value as desktop coordinates.
PanelPlacement import_placement(int gap_x, int gap_y,
                                const std::string& conf_output,
                                bool wayland,
                                const std::vector<MonitorRect>& monitors,
                                const std::string& fallback_output = "");

// Move a placement onto `target` while keeping its offset inside `current`
// (the monitor it sits on now), clamped so it is guaranteed to land within
// the new one. `current` may be nullptr, which places it at the target's
// top-left.
//
// Selecting an output has to carry the panel with it. Layer-shell margins are
// measured inside a single output, so a desktop position left outside the
// chosen output - 4100 on a 2560-wide monitor, say - would push the surface
// off screen entirely and the panel would simply vanish.
PanelPlacement retarget(const PanelPlacement& placement,
                        const MonitorRect* current,
                        const MonitorRect& target);
