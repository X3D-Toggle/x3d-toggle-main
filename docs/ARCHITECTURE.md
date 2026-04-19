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
└─ assets
    ├─ amd.svg
    ├─ ryzen.jpeg
    ├─ ryzenlogo.svg
    └─ ryzen.svg
└─ bin // Compiled Binaries
    ├─ x3d-daemon
    ├─ x3d-run
    └─ x3d-toggle
└─ build // Compiled Object Files/Artifacts
    ├─ bpf.h
    ├─ bpf.o
    ├─ ccd.c
    ├─ ccd.h
    ├─ config.h
    ├─ daemon.conf
    ├─ socket.c -> ~/x3d-toggle-main/src/socket.c
    ├─ socket.h -> ~/x3d-toggle-main/include/socket.h
    ├─ vmlinux.h
    ├─ worker.c -> ~/x3d-toggle-main/src/worker.c
    ├─ xui.c
    └─ xui.h
└─ config
    ├─ games.conf
    └─ settings.conf
└─ dev
    ├─ dev-install.sh
    ├─ dev-README.md
    ├─ dev-uninstall.sh
    ├─ logging
    │   ├─ audits
    │   └─ logs
    └─ sandbox
└─ docs
    ├─ ARCHITECTURE.md
    ├─ CODE_OF_CONDUCT.md
    ├─ DISCLAIMER
    ├─ ROADMAP.md
    ├─ SCRATCHPAD.md
    └─ x3d-toggle.1
└─ include
    ├─ cli.h
    ├─ cppc.h
    ├─ daemon.h
    ├─ error.h
    ├─ games.h
    ├─ libc.h
    ├─ misc.h
    ├─ modes.h
    ├─ socket.h
    ├─ status.h
    ├─ systemd.h
    └─ worker.h
└─ packaging
    ├─ 50-service.rules
    ├─ 99-sysfs.rules
    ├─ PKGBUILD
    ├─ sysusers.conf
    ├─ tmpfiles.conf
    ├─ toggle.desktop
    └─ toggle.service
└─ scripts
    ├─ framework
    │   ├─ assets.sh
    │   ├─ ccd.sh
    │   ├─ config.sh
    │   ├─ ebpftool.sh
    │   ├─ framework.sh
    │   ├─ policies.sh
    │   └─ xui.sh
    └─ tools
        ├─ debug.sh
        ├─ linter.sh
        ├─ reset.sh
        └─ rotate.sh
└─ src
    ├─ cli
    │   ├─ cli.c
    │   ├─ dialog.c
    │   └─ misc.c
    ├─ daemon
    │   ├─ bpf
    │   │   ├─ bpf.c
    │   │   ├─ bpf-user.c
    │   │   └─ bpf-user.h
    │   ├─ config.c
    │   ├─ cppc.c
    │   ├─ daemon.c
    │   ├─ diag.c
    │   ├─ modes.c
    │   └─ polling
    │       ├─ polling.c
    │       └─ polling.h
    ├─ error.c
    ├─ games.c
    ├─ libc.c
    ├─ run.c
    ├─ socket.c
    ├─ status.c
    ├─ stress.c
    ├─ sysfs.c
    ├─ systemd.c
    ├─ toggle.c
    └─ worker.c
─ CHANGELOG.md
─ compile_commands.json
─ CONTRIBUTING.md
─ deploy.sh
─ install.sh
─ LICENSE
─ Makefile
─ README.md
─ setup.sh
─ uninstall.sh
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
