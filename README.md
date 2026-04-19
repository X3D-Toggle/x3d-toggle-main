# AMD X3D V-Cache Technology Optimizer - v2.0.0

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Architecture: POSIX C](https://img.shields.io/badge/Architecture-POSIX%20C-orange.svg)](https://github.com/pyrotiger/x3d-toggle)
[![Engine: eBPF](https://img.shields.io/badge/Engine-eBPF-green.svg)](https://github.com/pyrotiger/x3d-toggle)

## X3D Toggle - Community Edition

A portable, high-performance utility for managing CCD priority on the AMD Ryzen X3D CPUs utilizing the 3D V-Cache Performance Optimizer under Linux via manual control or automated daemon.

### 🔭  Overview  🔭

This utility provides a graphical interface and automation to the `amd-x3d-vcache` kernel driver. It allows for real-time switching of the CPU scheduler bias to optimize for specific workloads on asymmetric dual-CCD processors (like 7950X3D/9950X3D).

* The Problem: CPPC Latency & Asymmetry
  Current CPPC drivers often fail to switch states deterministically, leading to micro-stutters.

* The Solution: x3d-toggle
  * **C Binary Backend**: The core logic is now a compiled C binary for instant execution and minimal system overhead. Sysfs node detection uses POSIX `glob()` for real-time latency reduction.
  * **Automated Daemon**: Real-time heuristics detect "Gaming" vs "Compute" loads.
  * **Manual Modes**: Instant user-defined priority via GUI or CLI.

### 📜  Prerequisites  📜

* **Linux Kernel**: 6.13 minimum (provides `amd-x3d-vcache` sysfs node); 7.0+ is recommended to take advantage of the daemon's advanced scheduling features (specifically, real-time CPPC dynamic preferred core hinting and zero-latency thread delegation during compute bursts). When coupled with the daemon (and planned eBPF refinements), this brings full feature parity of the Windows AMD 3D V-Cache Performance Optimizer driver to Linux.
* UEFI Configuration: CPPC Dynamic Preferred Cores set to [Driver].
* Build Dependencies: `clang`, `make`, `bpftool`.
* System Dependencies: `polkit`, `libbpf`. (Indirectly utilizes DBus via Polkit and optional GUI components).
* GUI Dependencies (Optional): `yad`, `kdialog`, `libnotify`.

### 🛡️  Architecture Security  🛡️

The utility interfaces with the sysfs node at `/sys/devices/platform/AMDI*/amd_x3d_mode` via PolicyKit.

* **C Binary**: Hardware writes are handled by `x3d-toggle` (installed to `/usr/bin`).
* **Polkit**: Actions are authorized via `org.x3dtoggle.policy`.

X3D Toggle follows a strict **Model-View-Controller (MVC)** philosophy:

* **Backend (Model):** Native C core interacting with `sysfs`, `amd_pstate`, and eBPF.
* **Daemon (Controller):** Orchestrates logic loops and handles IPC requests via UNIX Domain Sockets.
* **XUI (View):** A cross-platform UI logic layer (`x3d-xui`) that synchronizes iconography and formatting between Bash and C components.
* **Frontends (View):** CLI (`x3d-toggle`), Tray Icons, and optional Web/GTK interfaces.

For a deep dive into the system design, see [ARCHITECTURE.md](../documentation/ARCHITECTURE.md).

### 🖥️  XUI (X3D User Interface)  🖥️

The project utilizes a centralized UI definition system called **XUI**. This ensures that status messages, success/error states, and performance icons remain consistent across all interfaces:

* **Standardized Symbols**: 
  * 🐇 **Rabbit (Cache)**: Optimized for single-threaded gaming latency (CCD0).
  * 🐆 **Cheetah (Frequency)**: Optimized for multi-threaded compute throughput (CCD1).
  * ♊ **Dualize**: Both CCDs active for maximum width.
  * 🦅 **Boost**: High-performance state enabled.
* **Unified Logic**: UI tokens are defined in `x3d-xui.sh` and automatically injected into the C backend during the build process, ensuring perfect alignment between the CLI and the background daemon.

### 📦  Installation Methods 📦

#### 1. Arch Linux / Garuda (via Pre-compiled Pacman Package)

If you download the compiled Arch package directly from the **Releases** page (`.pkg.tar.zst`), you can install it seamlessly using `pacman` without needing to compile it yourself:

```bash
cd ~/Downloads/
sudo pacman -U x3d-toggle-*.pkg.tar.zst
```

#### 2. Arch Linux / Garuda (via Local PKGBUILD / Git Clone)

For developers or those wanting to compile directly from the absolute latest commits dynamically. The `makepkg` command automatically builds the C binary and wraps it into a `pacman` installation for you:

```bash
git clone https://github.com/pyrotiger/x3d-toggle.git
cd x3d-toggle
makepkg -si
```

#### 3. Manual Build (Make - Debian/Fedora/Ubuntu/Etc)

If you are running a non-Arch distro without `pacman` or `makepkg`:

```bash
git clone https://github.com/pyrotiger/x3d-toggle.git
cd x3d-toggle
make
sudo make install
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

Enable the system service to allow the background daemon to dynamically switch your system between Cache and Frequency profiles based on live workloads.

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

### 📟  CLI Command Lifecycle  📟

* Review [x3d-toggle.1](x3d-toggle.1) for more information

### 🚮  Uninstallation  🚮

To uninstall all binaries and assets, run the following:

```bash
cd to /path/to/folder/x3d-toggle
sudo make uninstall
```

---

### 🤝  Credits & Acknowledgments  🤝

* AMD — For the development of the X3D V-Cache technology.
* GrandBIRDLizard — For critical technical insights on sysfs node utilization, GPU IRQ management, and architectural guidance. Author of [X3Dctl](https://github.com/GrandBIRDLizard/x3dctl).

### ⚖️  Legal Disclaimer & Liability Limitation  ⚖️

**USE AT YOUR OWN RISK.** This utility interfaces directly with the Linux kernel and hardware sysfs nodes to modify CPU core scheduling and cache prioritization. By executing these scripts, deploying this package, or utilizing this software in any capacity, you acknowledge and agree to the following:

* **Warranty Voidance:** Manipulating CPU hardware states, frequencies, or standard driver behaviors outside of default operational parameters may void your processor or motherboard manufacturer warranties.
* **Liability:** The author and contributors shall not be held liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, hardware degradation, catastrophic system failure, data loss, or thermal issues) arising in any way out of the use of this software, even if advised of the possibility of such damage. The software is provided "as is" without any implied warranty.
* **Trademark:** AMD, Ryzen, and 3D V-Cache are trademarks of Advanced Micro Devices, Inc. This project is an independent community-led utility and is not affiliated with, endorsed by, or sponsored by AMD.

**Copyright ©️ 2026 Pyrotiger - distributed under the GPLv3 License**

* See [LICENSE](LICENSE) for more information.
