#pragma once

#include "placement.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Wiring between the placement record, UCCC's own config and the conky .conf
// files. Split out from placement.h (pure geometry) and config_parser.h
// (pure text) because this is the layer that needs the live screen layout
// and the active display server at the same time.
//
// app_config.json is the source of truth; the .conf is synced out to match.
namespace PlacementIO {

// Where a panel sits, in desktop coordinates. The stored record wins; with
// none yet, the .conf is imported so panels positioned before UCCC tracked
// placement still appear in the map instead of showing up unplaced.
// Read-only: nothing is persisted until save_placement().
PanelPlacement effective_placement(const std::string& panel, const fs::path& conf);

// Store the placement in app_config.json and write it out to the .conf so
// the two cannot disagree.
bool save_placement(const std::string& panel, const fs::path& conf,
                    const PanelPlacement& placement);

// Write an existing record out to the .conf alone, leaving app_config.json
// untouched.
bool sync_placement_to_conf(const fs::path& conf, const PanelPlacement& placement);

}  // namespace PlacementIO
