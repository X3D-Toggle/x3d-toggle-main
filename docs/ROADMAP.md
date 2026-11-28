# ROADMAP.md

## AMD X3D V-Cache Technology Optimizer

### Toggle Control - Community Edition

#### Codebase is a pure POSIX C utility (refer to `ARCHITECTURE.md`)

- [x] **Core Libraries for X3D Utility**
  - [x] Populate with build/make/install files (`Makefile`, `setup.sh`, `packaging/PKGBUILD`)
  - [x] Provide Mode Setting (`cache`, `frequency`, `dual`, `auto`)
  - [x] Set Elevated Permissions/Privileges (Polkit, `sysusers.conf`, `tmpfiles.conf`, udev `sysfs.rules`)
  - [x] Create matrix of CLI configurable options that is agnostic to front-ends
  - [x] Game Wrapper (`x3d-run`)

- [ ] **Daemon for Automation**
  - [x] Polling for Game/LLM loads
  - [x] Game List (`games.conf`)
  - [x] eBPF for deterministic detection at kernel level
    - [x] Ring buffer for kernel communication
    - [x] `sched_process_exec` tracepoint probe
    - [x] Depends on `libbpf`, `clang`, and `llvm`
  - [x] Unix Domain Socket (`/run/x3d-toggle/x3d-toggle.ipc`)
    - [x] IPC Robustness
    - [x] POSIX Signal Handling
  - [ ] Hysteresis Logic options
  - [x] Ensure daemon suspends itself until user resumes daemon if mode is manually set while daemon is running
  - [x] Versioning handshake
  - [x] Allow CLI to "attach" to the daemon and see live events showing CPU load changes and real-time eBPF triggers (`x3d-toggle monitor`)
  - [x] Fail-safe fallback if daemon errors

- [x] Run Valgrind and smoke test/linter/trial by fire
- [x] Add options for setting CPU Affinity (`sched_setaffinity`)
- [x] Add options for setting GPU IRQs
- [x] Clang/LLVM check (`clang-tidy`), `cppcheck`, `shellcheck`

- [ ] **Graphical UX**
  - [x] GTK4 / libadwaita Native Desktop Dashboard (`x3d-gui`)
  - [x] Kdialog - add linker to download version v1.x.x that provides the kdialog front end
  - [ ] WebUI (Go) - interfacing with the CLI, mimicking the GUI, this provides remote control/monitoring from a phone or other device over the network via SSH
  - [ ] KDE6 SNI (StatusNotifierItem / System Tray Icon)

- [ ] **Documentation**
  - [x] Update all documentation
  - [x] `README.md` and manpage (`x3d-toggle.1`) reflect all features/options available in standard user mode
  - [ ] `CHANGELOG.md` reflects full changes from v1.2.0/v1.3.0 to v2.0.0
  - [x] Legal framework is laid down (`LICENSE`, `DISCLAIMER`, `CODE_OF_CONDUCT.md`, `CONTRIBUTING.md`)

- [ ] **Developer Mode Environment Options & Documentation (`DEV_ENABLE=1`)**
  - [x] Logger — stdout / live journal format (`debug.sh`, `diag.c`)
  - [ ] Timer — measure switching latency
  - [x] Mock workload / stress script (`x3d-toggle stress`)
  - [ ] Hardware `Dry Run` mode

- [x] **AMD AOD (`amd-wmi-aod`) Integration** *(GUID `95F20B61-3D44-41F8-87E1-03CBF94C4E8F`)*
  - [x] Detect `/sys/bus/wmi/devices/95F20B61-3D44-41F8-87E1-03CBF94C4E8F*` and probe optional firmware-supported attributes
  - [x] Cache `mem_clock` (`MHz`), `smt_enable` (`0`/`1`), and `*_limit_{min,max,step}` in `x3d-daemon` on startup (avoid per-tick WMI/SMI calls) and expose via IPC
  - [x] Wire `ppt_limit` (`uW`, W × 1,000,000), `tdc_limit` (`mA`), `edc_limit` (`mA`), and firmware `smt_enable` (`0`/`1`) via IPC/GUI with reboot-required indicator (replaces legacy `cppc_tdp` stub)
  - [x] Provision root-only AOD sysfs nodes for `x3d-toggle` group via `tmpfiles.conf`, udev `sysfs.rules` (`DRIVER=="amd-wmi-aod"`), and `ReadWritePaths=-/sys/bus/wmi/devices` in `x3d-toggle.service`

**Copyright ©️ 2026 Pyrotiger - License: GPLv3**
