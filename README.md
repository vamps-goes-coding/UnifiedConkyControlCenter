# Unified Conky Control Center

**Never wrestle with Conky config files again.** A unified, visual control center that works seamlessly on both X11 and Wayland desktops—perfect if you love customizable system monitoring displays but hate managing scattered configuration files.

## Why This Exists

Conky is incredibly powerful for creating custom system monitor panels, but managing multiple `.conf` files across different display servers (X11 vs Wayland) gets messy fast. This tool brings everything into one clean interface so you can focus on what your panels *display* instead of where they're *stored*.

## What You Get

- **First-Run Setup**: We detect your system and ask where your Conky configs live. That's it—no digging through directories
- **Panel Control**: Start, stop, and restart panels with a click instead of terminal commands
- **Theme Manager**: Build and apply themes without wrestling with Lua syntax
- **Visual Gap Adjustment**: Tweak panel positioning with sliders, not config values
- **Quick Edit**: Open any config in your favorite editor—VS Code, Sublime, Vim, whatever
- **Batch Import**: Load multiple themes from a CSV file when you're setting up
- **Display Server Magic**: Automatically detects X11 vs Wayland and handles the differences
- **System Tray**: Quick access to controls from your desktop panel
- **CLI Mode**: Script it if you need headless operations

## Configuration

Everything's configurable without touching source code—either through the UI or by editing `~/.config/UnifiedConkyControlCenter/app_config.json`.

**Most users won't need to manually edit this**, but power users can:

- **Paths**: Where Conky configs and themes live (set via first-run setup, changeable in Preferences)
- **Display Server**: Auto-detected, but can override for X11/Wayland
- **Panel Discovery**: How we find your `.conf` files (prefix, extensions, exclusions)
- **UI Themes**: Choose from Light, Dark Charcoal, Dracula, Nord, and more
- **Editors**: Register your favorite text editor for opening configs
- **Refresh Intervals**: How often we check for panel status changes

Default paths are smart—they search common locations like `~/.config/conky` and adapt to your display server.

## Quick Start

### Option 1: Pre-built Packages (Easiest)

We provide ready-to-install packages for all major distributions:

- **Ubuntu/Debian**: `sudo dpkg -i unified-conky-control-center-1.0.28-Linux-x86_64.deb`
- **Fedora/RHEL/CentOS**: `sudo dnf install ./unified-conky-control-center-1.0.28-Linux-x86_64.rpm`
- **Arch Linux**: Download `PKGBUILD` and run `makepkg -si`
- **Any Linux**: Extract the generic archive: `tar xzf unified-conky-control-center-1.0.28-Linux-x86_64.tar.gz`
- **Smart Installer**: Detects your OS and installs automatically:
  ```bash
  curl -sSL https://raw.githubusercontent.com/vamps-goes-coding/UnifiedConkyControlCenter/master/install.sh | sudo bash
  ```

All packages available at: https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases

### Detailed Installation Instructions

**Ubuntu/Debian:**
```bash
wget https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v1.0.28/unified-conky-control-center_1.0.28_amd64.deb
sudo apt install ./unified-conky-control-center_*.deb
```

**Fedora/RHEL/CentOS:**
```bash
wget https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v1.0.1/unified-conky-control-center-1.0.0-1.x86_64.rpm
sudo dnf install ./unified-conky-control-center-*.rpm
```

**Arch Linux:**
```bash
# Download PKGBUILD and source
wget https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v1.0.1/PKGBUILD
wget https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v1.0.1/unified-conky-control-center-1.0.1-Linux-x86_64.tar.gz

# Build and install
makepkg -si
```

**Generic Linux (TGZ):**
```bash
wget https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases/download/v1.0.1/unified-conky-control-center-1.0.1-Linux-x86_64.tar.gz
mkdir -p ~/.local
tar xzf unified-conky-control-center-*.tar.gz -C ~/.local
~/.local/usr/local/bin/UnifiedConkyControlCenter
```

### Option 2: Build from Source

**Requirements:**
- CMake 3.21+
- Qt 6.4+
- C++20 compiler (GCC 10+, Clang 10+)
- Conky (runtime dependency)

**Install build dependencies:**

- **Ubuntu/Debian**: `sudo apt install build-essential cmake qt6-base-dev libqt6widgets6 nlohmann-json3-dev`
- **Fedora**: `sudo dnf install gcc-c++ cmake qt6-qtbase-devel nlohmann-json-devel`
- **Arch**: `sudo pacman -S base-devel cmake qt6-base nlohmann-json`

**Build:**
```bash
git clone https://github.com/vamps-goes-coding/UnifiedConkyControlCenter.git
cd Unified-Conky-Control-Center
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

**First time?** Just run it:
```bash
UnifiedConkyControlCenter
```

It'll guide you through selecting your Conky config folder.
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

Launch the application:
```bash
UnifiedConkyControlCenter
```

The setup wizard will ask you to select:
1. **Conky Config Folder** - Where your `.conf` files live (usually `~/.config/conky`)
2. **Themes Folder** - Where your `.lua` theme files are stored
3. **Display Server** - Whether you're using X11 or Wayland

These settings are saved automatically and can be changed later from the **Preferences dialog** → **Paths tab**.

### What You Can Do

**View all your Conky panels** - The main window displays every Conky configuration file it finds. See at a glance which panels are running and which are stopped.

**Start/stop panels individually** - Click to toggle any panel on or off. Changes take effect immediately.

**Edit configs graphically** - Click the "Edit" button to open configs in your preferred editor (VS Code, Vim, Nano, or others). Supports syntax highlighting for Conky syntax.

**Apply and manage themes** - Browse and preview Lua theme files, apply them to your panels, or create custom themes from templates.

**Monitor panel health** - The dashboard shows you which panels are running, their status, and any errors encountered.

**Customize the application itself** - Preferences dialog lets you configure application themes, editor choice, logging level, and default panels to start on launch.

### Command-Line Mode

For automation or scripting:
```bash
UnifiedConkyControlCenter --cli
```

### Environment Variable Overrides

For power users or automated setups, environment variables can override the default paths:

```bash
# Use a different Conky config directory
export CONKY_WAYLAND_DIR="$HOME/.config/conky-custom"
UnifiedConkyControlCenter

# Use a different themes directory
export CONKY_THEMES_DIR="$HOME/.config/conky/my-themes"
UnifiedConkyControlCenter

# Use a completely custom config file location
export CONKY_CONTROL_CENTER_CONFIG="$HOME/.config/my-app-config.json"
UnifiedConkyControlCenter
```

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

### "Application won't start"

Check your system has the required dependencies:
```bash
# Ubuntu/Debian
sudo apt install libqt6widgets6 libqt6core6
# Fedora
sudo dnf install qt6-qtbase
# Arch
sudo pacman -S qt6-base
```

### "Can't find my Conky config files"

1. **First run setup**: Delete `~/.config/UnifiedConkyControlCenter/` and restart — you'll be prompted to select your config folder
2. **Already configured**: Go to Preferences → Paths and click Browse to select the correct folder
3. **Check manually**: Your configs are usually in one of:
   - `~/.config/conky/`
   - `~/.config/conky/conky-wayland/`
   - Custom location set via environment: `echo $CONKY_WAYLAND_DIR`

### "Panels don't start when I click toggle"

1. Make sure Conky is installed: `which conky`
2. Try starting Conky manually to verify it works: `conky -c ~/.config/conky/conky-wayland-basic.conf`
3. Check application logs: In Preferences → Logging, enable Debug level and restart
4. Look for error messages in `~/.config/UnifiedConkyControlCenter/logs/`

### "Settings not saving between sessions"

1. Check folder permissions:
   ```bash
   ls -la ~/.config/UnifiedConkyControlCenter/
   chmod -R u+w ~/.config/UnifiedConkyControlCenter/
   ```
2. Ensure your home directory isn't read-only
3. Try running with explicit config path: `CONKY_CONTROL_CENTER_CONFIG=/tmp/test-config.json UnifiedConkyControlCenter`

### "Build fails on my system"

1. **Qt6 not found**: Install Qt6 development packages and set: `cmake .. -DCMAKE_PREFIX_PATH=/opt/qt6`
2. **CMake too old**: Install CMake 3.21+: https://cmake.org/download/
3. **Compiler missing**: Install GCC/Clang: `sudo apt install build-essential` (Ubuntu) or `sudo dnf install gcc-c++` (Fedora)
4. **See full error**: After cmake fails, check `build/CMakeOutput.log` and `build/CMakeError.log`

**Still stuck?** Open an issue at https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/issues with:
- Your OS and version
- Output of `cmake --version` and `qmake --version`
- The full error message from your build

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is open source. See the LICENSE file for details.

## Credits

Original Unified Conky Control Center by Mr.Vamps, inspired by the Conky Community.

## Website Links

Conky website: www.conky.cc
