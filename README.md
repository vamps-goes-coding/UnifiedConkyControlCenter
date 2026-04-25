# 🚀 Unified Conky Control Center (UCCC)

![Arch Linux](https://img.shields.io/badge/Arch_Linux-Verified-brightgreen?logo=arch-linux)
![CachyOS](https://img.shields.io/badge/CachyOS-Verified-brightgreen?logo=linux)
![Wayland](https://img.shields.io/badge/Wayland-Stable-brightgreen)
![Fedora](https://img.shields.io/badge/Fedora-Testing-yellow?logo=fedora)
![Debian](https://img.shields.io/badge/Debian-Needs_Tester-red?logo=debian)

**Stop wrestling with `.conf` files.** UCCC is a high-performance visual hub for managing your Conky panels across X11 and Wayland. Built for users who love deep desktop customization but hate scattered configuration.

[**Download Releases**](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/releases) | [**Report a Bug**](https://github.com/vamps-goes-coding/UnifiedConkyControlCenter/issues)

---

## 🛠 Features at a Glance

* **Zero-Config Discovery:** Automatically finds your panels in `~/.config/conky`.
* **One-Click Control:** Toggle, restart, or kill panels via a clean GUI or System Tray.
* **Theme Engine:** Build and swap Lua-based themes without touching code.
* **Visual Positioning:** Tweak gaps and alignment using sliders.
* **Smart Detection:** Seamlessly switches logic between X11 and Wayland environments.
* **CLI Mode:** Perfect for headless setups or startup scripting.

---

## 📥 Quick Install

### **Arch Linux & CachyOS (Recommended)**
```bash
# Get the PKGBUILD from releases, then:
makepkg -si