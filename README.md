# Unified Conky Control Center

A graphical interface for managing Conky panels, themes, and configurations. This version has been refactored to be fully configurable through JSON configuration files, making it easy for users to customize without modifying source code.

## Features

- **First-Run Setup**: On first launch, a setup dialog guides you through selecting your Conky configuration and themes folders
- **Panel Management**: Start, stop, and restart Conky panels
- **Theme Management**: Apply, create, and manage themes with color customization
- **Gap Adjustment**: Adjust panel positioning with gap controls
- **Editor Integration**: Open panel configurations in your preferred text editor
- **CSV Import/Export**: Bulk create themes from CSV files
- **System Tray Integration**: Quick access from system tray
- **CLI Mode**: Command-line interface for scripting

## Configuration

All settings are now configurable through `config/app_config.json`. This allows you to customize:

### Application Settings
- Display name, internal name, version, and organization

### Path Configuration
- Environment variables for Conky directories
- Default paths for Conky configurations and themes

### Panel Discovery
- Configuration file prefix (default: `conky-wayland-`)
- Configuration file extension (default: `.conf`)
- Files to exclude from panel discovery

### UI Settings
- Window dimensions (minimum and default size)
- Refresh intervals for heartbeat and panel status
- Default panels to start

### Theme Settings
- Theme file extension
- Theme file names (current.lua, categories.lua, etc.)

### App Themes
- Available UI themes (Dark Charcoal, Dracula, Nord, etc.)

### Editors
- List of available text editors with commands and icons

## Installation

### Prerequisites

 - CMake 3.21 or higher
 - Qt 6.4 or higher
 - C++20 compatible compiler (GCC 10+, Clang 10+)
- nlohmann_json library (automatically downloaded if not found)
 - **Runtime:** Conky must be installed on your system.

### Distro Dependencies

- **Ubuntu/Debian:** `sudo apt install build-essential cmake qt6-base-dev libqt6widgets6`
- **Fedora:** `sudo dnf install gcc-c++ cmake qt6-qtbase-devel`
- **Arch Linux:** `sudo pacman -S base-devel cmake qt6-base`

### Building from Source

```bash
# Clone the repository
cd Unified-Conky-Control-Center

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# Install (optional)
sudo make install
```

### ⚠️ Important: Copying to Another System

**DO NOT copy the `build/` directory** when transferring to another system. The build directory contains cached paths from your original system.

**To copy to another system:**
```bash
# From the project root, create a clean archive (excluding build/)
tar --exclude='build' -czvf UnifiedConkyControlCenter.tar.gz .

# Or copy only the source files
rsync -av --exclude='build' /path/to/Unified-Conky-Control-Center/ /destination/
```

**On the target system:**
```bash
# Extract and build fresh
tar -xzvf UnifiedConkyControlCenter.tar.gz
cd Unified-Conky-Control-Center
mkdir build && cd build
cmake ..
make
```

### Configuration

1. Copy the default configuration:
   ```bash
   cp config/app_config.json ~/.config/UnifiedConkyControlCenter/
   ```

2. Edit the configuration file to match your setup:
   ```bash
   nano ~/.config/UnifiedConkyControlCenter/app_config.json
   ```

3. Set environment variables (optional):
   ```bash
   export CONKY_WAYLAND_DIR="$HOME/.config/conky"
   export CONKY_THEMES_DIR="$HOME/.config/conky/themes"
   ```

## Usage

### First Run

On first launch, the application will show a setup dialog where you can:
1. Select your Conky configuration folder (containing .conf files)
2. Select your themes folder (containing .lua theme files)
3. Optionally create sample configuration files

The paths you select will be saved to `~/.config/UnifiedConkyControlCenter/app_config.json`.

### GUI Mode

```bash
./UnifiedConkyControlCenter
```

### CLI Mode

```bash
./UnifiedConkyControlCenter --cli
```

### Environment Variables

- `CONKY_WAYLAND_DIR`: Override the default Conky configuration directory
- `CONKY_THEMES_DIR`: Override the default themes directory
- `CONKY_CONTROL_CENTER_CONFIG`: Override the configuration file path

## Configuration File Structure

The `app_config.json` file has the following structure:

```json
{
  "application": {
    "display_name": "Unified Conky Control Center",
    "internal_name": "UnifiedConkyControlCenter",
    "version": "1.0.0",
    "organization": "Conky"
  },
  "paths": {
    "conky_wayland_dir_env": "CONKY_WAYLAND_DIR",
    "conky_themes_dir_env": "CONKY_THEMES_DIR",
    "default_conky_subpath": "conky-confs/conky-wayland",
    "default_themes_subpath": "themes"
  },
  "panel_discovery": {
    "config_prefix": "conky-wayland-",
    "config_extension": ".conf",
    "excluded_files": ["weather-location"]
  },
  "ui": {
    "window": {
      "min_width": 900,
      "min_height": 700,
      "default_width": 1000,
      "default_height": 750
    },
    "refresh_intervals": {
      "heartbeat_seconds": 10,
      "panel_status_seconds": 5
    },
    "default_panels_to_start": [
      "all-media",
      "basic-info",
      "calendar"
    ]
  },
  "themes": {
    "file_extension": ".lua",
    "current_theme_file": "current.lua",
    "preview_helper_file": "preview_helper.lua",
    "categories_file": "categories.lua",
    "current_theme_txt": "current_theme.txt"
  },
  "app_themes": [
    "Default Light",
    "Dark Charcoal",
    "Dracula",
    "Nord",
    "Solarized Light",
    "Oceanic"
  ],
  "editors": [
    {"name": "VS Code", "command": "code", "icon": "💠"},
    {"name": "Vim", "command": "vim", "icon": "⚙️"},
    {"name": "Nano", "command": "nano", "icon": "⌨️"}
  ]
}
```

## Customization Examples

### Adding a New Editor

Add to the `editors` array in `app_config.json`:

```json
{
  "name": "My Editor",
  "command": "my-editor",
  "icon": "📝"
}
```

### Changing Default Panels

Modify the `default_panels_to_start` array:

```json
"default_panels_to_start": [
  "my-custom-panel",
  "another-panel"
]
```

### Custom Theme File Extension

Change the theme file extension:

```json
"themes": {
  "file_extension": ".theme",
  ...
}
```

## Troubleshooting

### First Run Setup Not Appearing

If the first-run setup dialog doesn't appear:
1. Delete the settings file: `~/.config/UnifiedConkyControlCenter/`
2. Restart the application

### Configuration Not Loading

1. Check that the configuration file exists in one of these locations:
   - `./config/app_config.json` (current directory)
   - `~/.config/UnifiedConkyControlCenter/app_config.json`
   - `/etc/UnifiedConkyControlCenter/app_config.json`

2. Verify the JSON syntax is valid

3. Check the console output for error messages

### Panels Not Discovered

1. Verify the `config_prefix` matches your Conky configuration files
2. Check that the `config_extension` is correct
3. Ensure the Conky directory path is correct

### Themes Not Found

1. Verify the themes directory path
2. Check that theme files have the correct extension
3. Ensure the categories.lua file exists

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is open source. See the LICENSE file for details.

## Credits

Original Unified Conky Control Center by Mr.Vamps, inspired by the Conky Community.

## Website Links

Conky website: www.conky.cc
