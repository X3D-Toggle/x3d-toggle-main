# ARCHITECTURE.md

## AMD x3D V-Cache Technology Optimizer

### Toggle Control - Community Edition

* Pure POSIX C implementation for backend
* Modular Structure
  * Achieved by Model-View-Controller Hierarchy
* Policy of Least Privilege (udev via MAC)
* Mode switching is automated via Daemon which compliments the native CPPC
    driver
  * fallback Polling Heuristics Logic
  * eBPF deterministic detection
  * Gamelist database
* Providing fast CLI command to switch vcache persistent modes
* Providing user with optional graphical UX experiences
  * XUI - CLI environment
  * Logging Ability
* UNIX Domain Socket for IPC

```text
x3d-toggle-main
├── assets
│   ├── amd.svg
│   ├── ryzen.jpeg
│   ├── ryzen.jpg
│   ├── ryzenlogo.svg
│   ├── ryzen.svg
│   ├── x3d-toggle.jpg
│   ├── x3d-toggle.png
│   └── x3d-toggle.svg
├── bin (Compiled Binaries)
│   ├── x3d-daemon
│   ├── x3d-run
│   └── x3d-toggle
├── build (Build Artifacts)
│   ├── bpf.h
│   ├── bpf.o
│   ├── ccd.c
│   ├── ccd.h
│   ├── config.h
│   ├── daemon.conf
│   ├── irq_rules.stamp
│   ├── vmlinux.h
│   ├── xui.c
│   └── xui.h
├── CODE_OF_CONDUCT.md
├── compile_commands.json
├── config
│   ├── games.conf
│   ├── irq.conf
│   └── settings.conf
├── CONTRIBUTING.md
├── deploy.sh
├── dev
│   ├── dev-install.sh
│   ├── dev-README.md
│   ├── dev-uninstall.sh
│   ├── logging
│   │   ├── audits
│   │   ├── coredump
│   │   └── logs
│   ├── sandbox //future implementation
│   │   └── ide-install.json
│   └── toolsets
│       └── codeql
│           ├── codeql-config.yml
│           └── install-deps.sh
├── docs
│   ├── ARCHITECTURE.md
│   ├── CHANGELOG.md
│   ├── DISCLAIMER
│   ├── ROADMAP.md
│   ├── SCRATCHPAD.md
│   ├── x3d-toggle.1
│   └── x3d-toggle.1.md
├── include
│   ├── ccd.h -> ../build/ccd.h
│   ├── cli.h
│   ├── cppc.h
│   ├── daemon.h
│   ├── error.h
│   ├── games.h
│   ├── ipc.h
│   ├── irq.h
│   ├── libc.h
│   ├── misc.h
│   ├── modes.h
│   ├── scheduler.h
│   ├── status.h
│   ├── systemd.h
│   └── xui.h -> ../build/xui.h
├── install.sh
├── LICENSE
├── Makefile
├── meson.build
├── packaging
│   ├── 50-x3d_toggle-service.rules
│   ├── PKGBUILD
│   ├── sysfs.rules
│   ├── sysusers.conf
│   ├── tmpfiles.conf
│   ├── x3d-toggle.desktop
│   └── x3d-toggle.service
├── README.md
├── scripts
│   ├── framework
│   │   ├── assets.sh
│   │   ├── ccd.sh
│   │   ├── config.sh
│   │   ├── ebpftool.sh
│   │   ├── framework.sh
│   │   ├── gui.sh
│   │   ├── irq.sh
│   │   ├── policies.sh
│   │   └── xui.sh
│   └── tools
│       ├── archive.sh
│       ├── coredump.sh
│       ├── debug.sh
│       ├── linter.sh
│       ├── reset.sh
│       └── rotate.sh
├── setup.sh
├── src
│   ├── affinity.c
│   ├── cli
│   │   ├── cli.c
│   │   ├── dialog.c
│   │   └── misc.c
│   ├── daemon
│   │   ├── bpf
│   │   │   ├── bpf.c
│   │   │   ├── bpf.h -> ../../../build/bpf.h
│   │   │   ├── bpf-user.c
│   │   │   ├── bpf-user.h
│   │   │   └── vmlinux.h
│   │   ├── config.c
│   │   ├── cppc.c
│   │   ├── daemon.c
│   │   ├── diag.c
│   │   ├── modes.c
│   │   ├── polling
│   │   │   ├── polling.c
│   │   │   └── polling.h
│   │   └── steam.h
│   ├── error.c
│   ├── games.c
│   ├── gtk4
│   │   ├── gui.c
│   │   ├── stubs.c
│   │   ├── theme.css
│   │   ├── x3d-toggle-gui.gresource.xml
│   │   └── x3d-toggle.ui
│   ├── irq.c
│   ├── libc.c
│   ├── run.c
│   ├── scheduler.c
│   ├── socket.c
│   ├── status.c
│   ├── stress.c
│   ├── sysfs.c
│   ├── systemd.c
│   ├── toggle.c
│   └── worker.c
└── uninstall.sh
```
    
### 🧩  Component Breakdown  🧩

#### **1. Backend (Model)**

The Backend handles all raw interactions with the Linux kernel via the `amd-x3d-vcache` sysfs nodes. It consists of the `x3d-daemon` (which uses eBPF for zero-latency process detection) and low-level shell scripts for the final hardware write-ops.

#### **2. Conductor / Daemon (Controller)**

The daemon acts as the centralized brain (Controller). It listens for local IPC requests from frontends and monitors system heuristics (via `src/daemon/polling/`) or BPF events. It decides when to swap CCD priority based on detected "Gaming" vs "Compute" intents.

#### **3. Failsafe & Emergency Restoration**

A critical safety layer implemented in `sysfs.c` and enforced by `systemd.c` and `error.c`. If the daemon crashes or encounters terminal hardware state loss, an **async-signal-safe** routine forces the CPU back to "Balanced/Auto" mode using low-level syscalls.

#### **4. XUI (Shared View Layer)**

A unique feature of V2 is the **XUI** system. To ensure that the CLI, Daemon, and future WebUI all speak the same "visual language," the UI tokens (icons, colors, and step-formatting) are defined once in `x3d-xui.sh`. During compilation, these are injected into shared C headers and source files.

#### **5. Frontend (View)**

Frontends are modular and interchangeable. The primary `x3d-toggle` CLI routes commands through an IPC socket (`socket.c`) to the active daemon, ensuring that manual overrides are handled gracefully and persistently.

#### **Copyright ©️ 2026 Pyrotiger - License: GPLv3**
