# AMD X3D V-Cache Technology Optimizer - v2.0.1

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://github.com/X3D-Toggle/x3d-toggle-main/blob/main/LICENSE)
[![Platform: Linux Only](https://img.shields.io/badge/Platform-Linux%20Only-red.svg)](https://github.com/X3D-Toggle/x3d-toggle-main/blob/main/README.md)
[![Architecture: POSIX C](https://img.shields.io/badge/Architecture-POSIX%20C-orange.svg)](https://github.com/X3D-Toggle/x3d-toggle-main/blob/main/docs/ARCHITECTURE.md)
[![Engine: eBPF](https://img.shields.io/badge/Engine-eBPF-green.svg)](https://github.com/X3D-Toggle/x3d-toggle-main/blob/main/README.md)

> [!NOTE]
> **X3D Toggle v1.x** is a lightweight CLI/KDialog utility (available in [X3D Toggle v1](https://github.com/pyrotiger/x3d-toggle)), while **v2.x** provides a full GTK4/libadwaita GUI and daemon suite (available in [X3D Toggle v2](https://github.com/pyrotiger/x3d-toggle-main)).

> [!CAUTION]
> **Security Notice — Linux Only (Beware of Fake Windows Releases):**
> **X3D Toggle is strictly a Linux-only application.** There is **no** Windows version, and this project will never distribute Windows executables. AMD develops their own software, **AMD 3D V-Cache Performance Optimizer Driver**, for use on Windows platforms, but it is **NOT** the same program as X3D Toggle.
>
> Any Windows `.exe` or `.zip` files circulating online or hosted on cloned/unofficial repositories claiming to be X3D Toggle are **unofficial, potentially malicious, and must NOT be trusted or executed**.
>
> Only obtain X3D Toggle from this official repository, its official Linux releases (`.pkg.tar.zst` / source), or the official [AUR package](https://aur.archlinux.org/packages/x3d-toggle).

## X3D Toggle - Community Edition

A portable, high-performance utility for managing CCD priority on the AMD Ryzen X3D CPUs utilizing the 3D V-Cache Performance Optimizer under Linux via manual control or automated daemon.

### 🔭  Overview  🔭

This utility provides a native GTK4 desktop dashboard and automation backend for the `amd_x3d_vcache`, `amd-pstate`, and upcoming `amd-wmi-aod` kernel drivers. It allows for real-time switching of CPU scheduler bias, SMT-aware L3 cgroup isolation, BORE/EEVDF tuning, IRQ affinity steering, and scaffolded AMD Overclocking (AOD) WMI controls across the AMD Ryzen X3D lineup:
* **Asymmetric Dual-CCD X3D:** Ryzen 9 9950X3D, 9900X3D, 7950X3D, 7900X3D
* **Symmetric Dual 3D V-Cache:** Ryzen 9 9950X3D2 (192 MB Dual V-Cache)
* **Single-CCD X3D:** Ryzen 7 9850X3D, 9800X3D, 7800X3D (unified V-Cache)

* The Problem: CPPC Latency & Asymmetry
  Current CPPC drivers often fail to switch states deterministically, leading to micro-stutters.

* The Solution: x3d-toggle
  * **C Binary Backend**: The core logic is now a compiled C binary for instant execution and minimal system overhead. Sysfs node detection uses POSIX `glob()` for real-time latency reduction.
  * **Automated Daemon**: Real-time heuristics detect "Gaming" vs "Compute" loads.
  * **Manual Modes**: Instant user-defined priority via GUI or CLI.
  * **AMD AOD WMI Ready**: Pre-scaffolded for AMD's new WMI-based Overclocking Driver (`amd-wmi-aod`), unlocking firmware power/clock/SMT tuning automatically once deployed in the host kernel.

### 📜  Prerequisites  📜

* **Linux Kernel**: 6.13 minimum (provides `amd_x3d_vcache` sysfs node); **7.0 – 7.3+** is fully supported and recommended (native EEVDF + modern BORE `sched_burst_penalty_offset` / `sched_burst_inherit_type`, `amd_pstate` active CPPC EPP, and CO-RE eBPF process tracking).
* UEFI Configuration: CPPC Dynamic Preferred Cores set to `[Driver]` (for asymmetric dual-CCD X3D models).
* Build Dependencies: `clang`, `make`, `bpftool` (plus `pkgconf`, `glib2-devel`, `gtk4`, and `libadwaita` development headers when compiling the optional `x3d-gui` binary).
* System Dependencies: `polkit`, `libbpf`.
* GUI Dependencies (Optional): `gtk4`, `libadwaita` (for the `x3d-gui` binary, packaged as `x3d-toggle-gui`), or `kdialog` (for lightweight `x3d-toggle kdialog` / `x3d-toggle-kdialog`).
* Firmware Overclocking Interface (Optional): `amd-wmi-aod` kernel module (currently in upstream review; automatically detected when deployed).

### 🛡️  Architecture Security  🛡️

The utility interfaces with the sysfs node at `/sys/devices/platform/AMDI*/amd_x3d_mode` via PolicyKit.

* **C Binary**: Hardware writes are handled by `x3d-toggle` (installed to `/usr/bin`).
* **Polkit**: Actions are authorized via `org.x3dtoggle.policy`.

X3D Toggle follows a strict **Model-View-Controller (MVC)** philosophy:

* **Backend (Model):** Native C core interacting with `sysfs`, `amd_pstate`, and eBPF.
* **Daemon (Controller):** Orchestrates logic loops and handles IPC requests via UNIX Domain Sockets.
* **XUI (View):** A cross-platform UI logic layer (`x3d-xui`) that synchronizes iconography and formatting between Bash and C components.
* **Frontends (View):** CLI (`x3d-toggle`), Tray Icons, and optional Web/GTK interfaces.

For a deep dive into the system design, see [ARCHITECTURE.md](docs/ARCHITECTURE.md).

### 🖥️  XUI (X3D User Interface)  🖥️

The project utilizes a centralized UI definition system called **XUI**. This ensures that status messages, success/error states, and performance icons remain consistent across all interfaces:

* **Standardized Symbols**: 
  * 🐇 **Rabbit (Cache)**: Optimized for single-threaded gaming latency (CCD0).
  * 🐆 **Cheetah (Frequency)**: Optimized for multi-threaded compute throughput (CCD1).
  * ♊ **Dualize**: Both CCDs active for maximum width.
  * 🦅 **Boost**: High-performance state enabled.
* **Unified Logic**: UI tokens are defined in `x3d-xui.sh` and automatically injected into the C backend during the build process, ensuring perfect alignment between the CLI and the background daemon.

### 📦  Installation Methods 📦

#### 1. Arch Linux / CachyOS / Garuda (via AUR — Recommended)

Install directly from the [AUR](https://aur.archlinux.org/packages/x3d-toggle) (`https://aur.archlinux.org/x3d-toggle.git`):

* **Base CLI + Daemon only (headless / minimal):**

  ```bash
  paru -S x3d-toggle
  # or
  yay -S x3d-toggle
  ```

* **Full Desktop (GTK4/libadwaita GUI + Base CLI/Daemon):**

  ```bash
  paru -S x3d-toggle-gui
  # or
  yay -S x3d-toggle-gui
  ```

* **Or build manually from the AUR repo:**

  ```bash
  git clone https://aur.archlinux.org/x3d-toggle.git
  cd x3d-toggle
  makepkg -si
  ```

#### 2. Arch Linux / Garuda (via Pre-compiled Pacman Package)

If you download the compiled Arch package directly from the **Releases** page (`.pkg.tar.zst`), you can install it seamlessly using `pacman` without needing to compile it yourself:

```bash
cd ~/Downloads/
sudo pacman -U x3d-toggle-*.pkg.tar.zst
```

#### 3. Arch Linux / Garuda (via Local PKGBUILD / Git Clone)

For developers or those wanting to compile directly from the absolute latest commits dynamically. The `makepkg` command automatically builds the C binary and wraps it into a `pacman` installation for you:

```bash
git clone https://github.com/X3D-Toggle/x3d-toggle-main.git
cd x3d-toggle-main/packaging
makepkg -si
```

#### 4. Manual Build (Make - Debian/Fedora/Ubuntu/Etc)

If you are running a non-Arch distro without `pacman` or `makepkg`:

```bash
git clone https://github.com/X3D-Toggle/x3d-toggle-main.git
cd x3d-toggle-main
make
sudo make install setup
# Optional: build/install bin/x3d-gui directly when gtk4 + libadwaita dev headers are present
sudo make gui
```

### 🎮  Application Usage  🎮

* Launch the GUI/Interface via your application launcher (search for "X3D CCD Control") or execute via terminal:

  ```bash
  x3d-toggle
  ```

* Accepted Application/Desktop Launcher Keywords: `x3d` `x3d-toggle` `vcache` `cpu` `rabbit` `cheetah` `llm` `encode` `streaming` `workload` `compute` `elk`
* **Note on Desktop Shortcuts:** In Arch Linux, packages securely place their `.desktop` files in your Application Launcher (`/usr/share/applications/`), rather than forcing icons onto your physical Desktop. If you prefer a literal shortcut icon on your Desktop, simply run this command:

  ```bash
  cp /usr/share/applications/x3d-toggle.desktop ~/Desktop/ && chmod +x ~/Desktop/x3d-toggle.desktop
  ```

### ⚙️  Background Daemon & Utilities  ⚙️

#### Automated Scheduling Service 🎹

Enable the system service to allow the background daemon to dynamically switch your system between Cache and Frequency profiles based on live workloads:

**Via Systemd:**

```bash
sudo systemctl enable --now x3d-toggle.service
```

**Via X3D CLI (Recommended):**

```bash
x3d start   # Hard Wake: Enable autostart + Start session
x3d enable  # Soft Wake: Start session only
```

#### Configuration Overrides  🔧

To fine-tune the daemon's behavior, edit the **Default Configuration** file `/etc/x3d-settings.conf` (Requires `sudo`).
More advanced, persistent overrides can be found in the **User Configuration** directory at `/etc/x3d-toggle.d`.

* **POLL_INTERVAL:** Adjust how frequently the daemon checks for state changes in seconds (Default: 3).
* **COMPUTE_LOAD_THRESHOLD:** Set the CPU usage percentage required to trigger Cheetah/Frequency mode (Default: 50).
* **GAME_DETECTION_MODE:** Select the method used to detect Gaming/Rabbit mode intent (Default: 2).
  * Mode `1`: Simplified legacy detection (Gamemoded + Steam).
  * Mode `2`: Advanced dynamic detection. Caches your installed application metadata (`.desktop` fields) and continually matches it against `top` resource utilization and steam usage.

* **Note:** After editing, restart the daemon to apply changes:

   ```bash
   sudo systemctl restart x3d-toggle.service
   ```

### ⚡  AMD Overclocking (AOD) WMI Support  ⚡

X3D Toggle supports AMD's new Linux **AMD Overclocking Driver (`amd-wmi-aod`)**, which bridges Ryzen platform firmware tuning to `sysfs` over ACPI WMI (GUID `95F20B61-3D44-41F8-87E1-03CBF94C4E8F`):

* **Upstream Status & Conditional Scaffolding:** `amd-wmi-aod` is currently in upstream Linux kernel review. X3D Toggle is already scaffolded end-to-end (daemon IPC caching, `tmpfiles.d`/udev group provisioning, and GTK4 UI) and performs a runtime host feature check so AOD controls activate seamlessly as soon as the WMI interface is present in your kernel.
* **Live Dashboard Telemetry (Active Today):** A dedicated **Power & Current Limits** widget on the dashboard displays real-time **PPT**, **TDC**, **EDC**, and **Mem Clock** telemetry (plus **AOD Status**) regardless of whether `amd-wmi-aod` is loaded yet; clicking it routes to **AOD Controls** when the WMI driver is present (or displays an upstream-review status view until your kernel includes it).
* **Firmware Tuning Controls (Unlocked with `amd-wmi-aod` + Advanced Options):** Provides paired slider + manual numeric inputs and one-click CPU stock defaults for **PPT Limit (W)**, **TDC Limit (A)**, **EDC Limit (A)**, and **Mem Clock (MHz)**, plus a conditional **IRQ SMT** widget inside **AOD Controls**. *(Because AOD updates UEFI setup variables, a system reboot is required for firmware changes to take effect.)*

### 📟  CLI Command Lifecycle  📟

* Review [x3d-toggle.1](docs/x3d-toggle.1) for more information

### 🚮  Uninstallation  🚮

To uninstall all binaries and assets, run the following:

```bash
cd /path/to/x3d-toggle-main
sudo make purge
```

---

### 🤝  Credits & Acknowledgments  🤝

* AMD — For the development of the X3D V-Cache technology.
* GrandBIRDLizard — For critical technical insights on sysfs node utilization, GPU IRQ management, and architectural guidance. Author of [X3Dctl](https://github.com/GrandBIRDLizard/x3dctl).
* LACT and CoreCtrl are independent third-party projects; X3D Toggle is not affiliated with them and only provides optional launch shortcuts when they are already installed.

### ⚖️  Legal Disclaimer & Liability Limitation  ⚖️

**USE AT YOUR OWN RISK.** This utility interfaces directly with the Linux kernel and hardware sysfs nodes to modify CPU core scheduling and cache prioritization. By executing these scripts, deploying this package, or utilizing this software in any capacity, you acknowledge and agree to the following:

* **Warranty Voidance:** Manipulating CPU hardware states, frequencies, or standard driver behaviors outside of default operational parameters may void your processor or motherboard manufacturer warranties.
* **Liability:** The author and contributors shall not be held liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, hardware degradation, catastrophic system failure, data loss, or thermal issues) arising in any way out of the use of this software, even if advised of the possibility of such damage. The software is provided "as is" without any implied warranty.
* **Trademark:** AMD, Ryzen, and 3D V-Cache are trademarks of Advanced Micro Devices, Inc. This project is an independent community-led utility and is not affiliated with, endorsed by, or sponsored by AMD.
* See [DISCLAIMER](DISCLAIMER) for more information.

**Copyright ©️ 2026 Pyrotiger - distributed under the GPLv3 License**

* See [LICENSE](LICENSE) for more information.
