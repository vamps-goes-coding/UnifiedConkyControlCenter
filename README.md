# Unified Conky Control Center (UCCC)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Version](https://img.shields.io/badge/version-1.1.13-green.svg)](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases)

Works on **any Linux distro** - Works on **X11 + Wayland** - Works on **every desktop environment**

**Stop wrestling with `.conf` files.** UCCC is a simple visual hub for managing all your Conky panels. Built for everyone who loves customizing their desktop, without the headache.

[**Download Releases**](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases) | [**Report Issues**](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/issues)

---

## Screenshots

### Main Window
![Theme Manager](screenshots/theme-manager.png)

### Theme Control
![Theme Control](screenshots/theme-control.png)

### Preferences
![Preferences Window](screenshots/preferences-window.png)

### System Tray Menu
![Tray Menu](screenshots/tray-menu.png)

---

## What it does

- Automatically finds all your existing Conky panels
- One click to start / stop / restart any panel
- Theme switcher without editing code
- Works silently in your system tray
- No complicated setup required
- Works exactly the same everywhere

---

## Requirements

| Dependency | Purpose |
|-----------|---------|
| Qt6 >= 6.4 | GUI framework |
| CMake >= 3.21 | Build system |
| Conky | The thing this manages |
| nlohmann_json | Config file handling (auto-fetched if missing) |

---

## Install

### Arch Linux

There is not yet an AUR package under this name — install from the bundled
`PKGBUILD`, which downloads the release tarball:

```bash
git clone https://github.com/vamps-goes-coding/UnifiedConkyControlCenter.git
cd UnifiedConkyControlCenter
makepkg -si
```

> The `PKGBUILD` pulls `UnifiedConkyControlCenter-<version>-Linux-x86_64.tar.gz`
> from the [Releases](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases)
> page, so make sure that release exists before building.

### Debian / Ubuntu

Download the `.deb` from [Releases](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases) and install:

```bash
sudo dpkg -i unified-conky-control-center_*.deb
sudo apt-get install -f
```

### Fedora / RPM-based

Download the `.rpm` from [Releases](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases) and install:

```bash
sudo rpm -i unified-conky-control-center-*.rpm
```

### Universal Installer

```bash
curl -sSL https://raw.githubusercontent.com/vamps-goes-coding/UnifiedConkyControlCenter/master/install.sh | bash
```

---

## Building from Source

```bash
git clone https://github.com/vamps-goes-coding/UnifiedConkyControlCenter.git
cd UnifiedConkyControlCenter
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

The version is taken from `git describe --tags`. Building from a tarball or a
shallow clone without tags falls back to the version in `CMakeLists.txt`; you
can also pin it explicitly with `-DAPP_VERSION_OVERRIDE=1.2.3`.

---

## Verifying a Build

Run the unit tests (no display server needed):

```bash
ctest --test-dir build --output-on-failure
```

The binary also ships its own self-check, which builds the real window, walks
both modes and refreshes every tab:

```bash
./build/UnifiedConkyControlCenter --smoke-test   # exit 0 = pass
```

`scripts/smoke-test.sh` wraps that under a real display server and also
asserts on the session log — this is what CI runs:

```bash
./scripts/smoke-test.sh build/UnifiedConkyControlCenter x11
./scripts/smoke-test.sh build/UnifiedConkyControlCenter wayland
```

---

## How to Use

1. Quick and easy setup from the Smart Installer (install.sh)
2. Launch it from your applications menu
3. All your Conkys will show up automatically
4. Click the buttons to control them
5. Right click the tray icon for quick actions

You don't need to configure anything. It just works.

---

## Supported Environments

Gnome, KDE, Xfce, Cinnamon, MATE, LXQt, i3, Sway, Hyprland, Openbox, Awesome, BSPWM, and everything else that runs Linux.

---

## Contributing

Contributions are welcome! If you find a bug or want to add a feature:

1. Fork the repo
2. Create a branch (`git checkout -b my-feature`)
3. Commit your changes
4. Open a Pull Request

---

## Support

If you run into issues or have questions, [open an issue](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/issues) on GitHub.

---

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

---

Thank you for checking out this tool. I hope it helps you as it has me.
