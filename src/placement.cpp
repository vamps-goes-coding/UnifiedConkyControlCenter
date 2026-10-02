#include "placement.h"

#include <algorithm>
#include <cstdint>

// ── Monitor lookup ───────────────────────────────────────────────────────────

const MonitorRect* monitor_for_point(const std::vector<MonitorRect>& monitors,
                                     int px, int py) {
    const MonitorRect* best = nullptr;
    for (const auto& m : monitors) {
        if (px < m.x || py < m.y) continue;
        if (px >= m.x + m.w || py >= m.y + m.h) continue;
        // Smallest containing monitor wins so a point in an overlap always
        // picks the same one regardless of how the list was built.
        if (best == nullptr ||
            static_cast<std::int64_t>(m.w) * m.h <
                static_cast<std::int64_t>(best->w) * best->h) {
            best = &m;
        }
    }
    return best;
}

const MonitorRect* nearest_monitor(const std::vector<MonitorRect>& monitors,
                                   int px, int py) {
    const MonitorRect* best = nullptr;
    std::int64_t best_d = 0;
    for (const auto& m : monitors) {
        const std::int64_t dx =
            (m.x + m.w / 2) - static_cast<std::int64_t>(px);
        const std::int64_t dy =
            (m.y + m.h / 2) - static_cast<std::int64_t>(py);
        const std::int64_t d = dx * dx + dy * dy;
        if (best == nullptr || d < best_d) {
            best = &m;
            best_d = d;
        }
    }
    return best;
}

const MonitorRect* monitor_by_name(const std::vector<MonitorRect>& monitors,
                                   const std::string& name) {
    if (name.empty()) return nullptr;
    for (const auto& m : monitors) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

const MonitorRect* primary_monitor(const std::vector<MonitorRect>& monitors) {
    for (const auto& m : monitors) {
        if (m.primary) return &m;
    }
    return nullptr;
}

ResolvedPoint resolve_output_point(const std::vector<MonitorRect>& monitors,
                                   int px, int py) {
    ResolvedPoint out;
    const MonitorRect* m = monitor_for_point(monitors, px, py);
    if (m == nullptr) m = nearest_monitor(monitors, px, py);
    out.monitor = m;
    if (m != nullptr) {
        const auto rel = to_relative(*m, px, py);
        out.rel_x = rel.first;
        out.rel_y = rel.second;
    }
    return out;
}

DesktopBox desktop_bounds(const std::vector<MonitorRect>& monitors) {
    DesktopBox box;
    if (monitors.empty()) return box;

    int x1 = monitors.front().x;
    int y1 = monitors.front().y;
    int x2 = monitors.front().x + monitors.front().w;
    int y2 = monitors.front().y + monitors.front().h;

    for (const auto& m : monitors) {
        x1 = std::min(x1, m.x);
        y1 = std::min(y1, m.y);
        x2 = std::max(x2, m.x + m.w);
        y2 = std::max(y2, m.y + m.h);
    }
    box.x = x1;
    box.y = y1;
    box.w = x2 - x1;
    box.h = y2 - y1;
    return box;
}

// ── Coordinate conversion ────────────────────────────────────────────────────

std::pair<int, int> to_relative(const MonitorRect& m, int x, int y) {
    return {x - m.x, y - m.y};
}

std::pair<int, int> to_desktop(const MonitorRect& m, int rel_x, int rel_y) {
    return {m.x + rel_x, m.y + rel_y};
}

// ── Import ───────────────────────────────────────────────────────────────────

PanelPlacement import_placement(int gap_x, int gap_y,
                                const std::string& conf_output,
                                bool wayland,
                                const std::vector<MonitorRect>& monitors,
                                const std::string& fallback_output) {
    PanelPlacement p;
    p.output = !conf_output.empty() ? conf_output : fallback_output;

    // X11: conky positions absolutely, so the gaps are already desktop
    // coordinates and there is nothing to lift.
    if (!wayland) {
        p.x = gap_x;
        p.y = gap_y;
        return p;
    }

    // Wayland: the gaps are margins inside whichever output the panel is
    // mounted on, so that output's origin has to be added back.
    const MonitorRect* target = monitor_by_name(monitors, p.output);

    if (target == nullptr && !monitors.empty()) {
        // The named output is not in the current layout (unplugged, renamed),
        // or none was named: resolve to a monitor that actually exists so the
        // stored record never points at a phantom. The ambient output gets
        // the next say, then primary, then the first monitor - conky's own
        // fallback chain.
        target = monitor_by_name(monitors, fallback_output);
        if (target == nullptr) target = primary_monitor(monitors);
        if (target == nullptr) target = &monitors.front();
        p.output = target->name;
    }

    if (target == nullptr) {
        // No layout to convert against - take the gaps at face value.
        p.x = gap_x;
        p.y = gap_y;
        return p;
    }

    const auto desktop = to_desktop(*target, gap_x, gap_y);
    p.x = desktop.first;
    p.y = desktop.second;
    return p;
}
