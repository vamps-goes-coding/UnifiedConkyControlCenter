#include "placement_io.h"

#include "config_manager.h"
#include "config_parser.h"
#include "screens.h"

#include <cstdlib>
#include <iostream>

namespace PlacementIO {
namespace {

// The session-wide output a user may have exported (e.g. from
// ~/.config/plasma-workspace/env/). Only consulted when the .conf names no
// output of its own, so it can never override a per-panel setting.
std::string ambient_output() {
    const char* value = std::getenv("CONKY_WAYLAND_OUTPUT");
    return value != nullptr ? std::string(value) : std::string();
}

bool is_wayland() {
    return ConfigManager::instance().get_active_display_server_key() == "wayland";
}

}  // namespace

PanelPlacement effective_placement(const std::string& panel, const fs::path& conf) {
    auto& config = ConfigManager::instance();

    // The stored record is the source of truth once UCCC has one.
    if (config.has_placement(panel)) {
        return config.get_placement(panel);
    }

    // First open: lift what the .conf already says into desktop coordinates.
    // Deliberately not persisted - the .conf keeps being the truth until the
    // user actually moves the panel, so an external edit stays visible.
    int gap_x = 0;
    int gap_y = 0;
    std::string output;
    try {
        gap_x = ConfigParser::get_gap_x(conf);
        gap_y = ConfigParser::get_gap_y(conf);
        output = ConfigParser::get_wayland_output(conf);
    } catch (const std::exception& e) {
        std::cerr << "WARNING: could not read placement for panel '" << panel
                  << "': " << e.what() << std::endl;
        return PanelPlacement{};
    }

    return import_placement(gap_x, gap_y, output, is_wayland(),
                            Screens::monitors(), ambient_output());
}

bool save_placement(const std::string& panel, const fs::path& conf,
                    const PanelPlacement& placement) {
    auto& config = ConfigManager::instance();
    config.set_placement(panel, placement);
    if (!config.save_config()) {
        std::cerr << "WARNING: placement for panel '" << panel
                  << "' was written to " << conf
                  << " but could not be persisted to app_config.json" << std::endl;
    }
    return sync_placement_to_conf(conf, placement);
}

bool sync_placement_to_conf(const fs::path& conf, const PanelPlacement& placement) {
    // output_origin already answers (0,0) for X11, so this one subtraction
    // covers both servers: verbatim desktop coordinates on X11, output-
    // relative margins on Wayland. Nothing is clamped - layer-shell accepts
    // negative margins, and clamping is what used to pin panels to a
    // monitor's edge.
    const auto [origin_x, origin_y] = Screens::output_origin(placement.output);
    const int file_x = placement.x - origin_x;
    const int file_y = placement.y - origin_y;

    if (!ConfigParser::set_gap_values(conf, file_x, file_y)) {
        return false;
    }

    // wayland_output is a Wayland-only setting. Keep it present exactly when
    // the active server reads it, so switching servers cannot leave a stale
    // output pinned to a panel - and so it is there for conky to prefer over
    // the ambient session variable.
    const std::string output = is_wayland() ? placement.output : std::string();
    return ConfigParser::set_wayland_output(conf, output);
}

}  // namespace PlacementIO
