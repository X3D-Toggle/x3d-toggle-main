# **X3D Toggle — UI Architecture & Layout Specification**

## **Global Contracts & Controls**

* **Frontend MVC Architecture (`src/gtk4/`):**
  * **Model (`gui_model.c` / `gui_model.h`):** Thin `x3d-daemon` UNIX IPC client (`socket_send()`), GObject reactive state, user GUI preferences (`gui.conf`), and UI-only high-frequency GPU/per-core visualizer readers (zero duplicate daemon/sysfs pipelines and zero GTK widget dependencies).
  * **View (`gui_view.c` / `gui_widgets.c` / `gui_popups.c` + `x3d-toggle.ui` + CSS):** Declarative widget hierarchy, reusable widget mode containers, universal sidebar `*CallToAction*` bar, 3-column centered/justified grids, `-Toggle` pills, `-PopUp` window shells, and CSS theme application (no direct IPC/sysfs mutations).
  * **Controller (`gui_controller.c` / `gui_controller_priv.h`):** Lifecycle, navigation state machine, Dirty-State Safeguard dialogs, live polling timers, sidebar gating, and page data refreshers.
  * **Handlers (`gui_handlers.c` / `gui_handlers.h` + `gui_stubs.c`):** Isolated UI widget signal handler blocks grouped by page, plus quarantined `gui_stubs.c` placeholders for Phase 6 feature addons.
  * **Bootstrap (`gui.c`):** Lean `AdwApplication` entry point wiring Model, View, and Controller.
* **Widget Modes (`GtkWidget` / `GObject` Subclasses):**
  * **`X3dCurrentWidget` (`.current-widget`):** Read-only live status and telemetry subclass; supports optional capability-flag visibility binding.
  * **`X3dStagedWidget` (`.staged-widget`):** Derivable/reusable `GtkWidget` subclass exposing a native `dirty` `GObject` boolean property, standard `"default"` and `"cancel"` signals, a boolean-returning `"apply"` signal (`TRUE` on commit success, `FALSE` to preserve `dirty` on IPC/sysfs failure), `"close"` in standalone mode, an embedded 3-column centered/justified nested grid, and a built-in bottom-right `*CallToAction*` bar. Buffers all edits until **Apply** is pressed; **Cancel** restores active runtime values without closing the view; **Default** loads stock/default values without committing to sysfs/daemon.
  * **`X3dImmediateWidget` (`.immediate-widget`):** Immediate-dispatch `GtkWidget` subclass with built-in 3-column nested grid; applies immediately on click/toggle (no CallToAction bar).
  * **`X3dDangerWidget` (`.danger-widget`):** High-contrast destructive-action `GtkWidget` subclass used for **EMERGENCY OVERRIDE** with built-in `AdwAlertDialog` (`adw_dialog_present()`) confirmation prompt gating.
  * **Capability-Bound Visibility:** Widgets and controls (`PPT`, `TDC`, `EDC`, `Mem Clock`, `IRQ SMT`, `dynamic_epp`, `amd_pstate_floor_freq`, `BORE`, etc.) bind visibility/sensitivity directly to `X3dGuiModel` capability properties (`caps` + `force-unsupported`).
* **Universal Sidebar CallToAction Bar (\*CallToAction\*):** Placed at the bottom of the navigation sidebar beneath a horizontal divider, with each action rendered as a full-width stacked bar (`Apply`, then `Cancel`, then `Default`).
  * **Apply:** Commits all dirty staged widgets on the active page to the system (closes standalone System Profile view on success).
  * **Cancel:** Discards staged edits on the active page and restores active runtime values (keeps standalone views open).
  * **Default:** Loads stock/default values into all staged widgets on the active page without committing to sysfs/daemon.
  * **Close *(Standalone View Add-on)*:** Runs Cancel semantics and closes the standalone view (shown only on standalone Dashboard subviews).
* **Sidebar Structure & Dividers:**
  * `Dashboard` -> `CPU` -> `GPU` -> *[Divider]* -> `Thread Settings` -> `Daemon Settings` -> *(nested sub-items: `Advanced Configuration`, `Developer Options`)* -> *[Divider]* -> `Profile Manager` -> `Extras` -> *[Divider]* -> Universal `*CallToAction*` (`Apply` / `Cancel` / `Default`).
* **Popout Telemetry Visualizers:** Clicking directly on any live telemetry dashboard card (**CPU Dashboard**, **GPU Dashboard**, or Dashboard **Power & Current Limits Status**) launches a detached live visualizer window supporting 7 switchable modes: **Text**, **Graph**, **Pie**, **Chart**, **Breakdown**, **Polygon**, and **Concentric Ring**.
* **Control Primitives:**
  * **\-Toggle:** Touchscreen-friendly button enveloping the label; flips between passive and active theme colors. Staged inside Staged Widgets; immediate inside Immediate Widgets.
  * **\-LiveToggle:** Immediate-widget toggle behavior—applies on press, no Apply required.
  * **\-PopUp:** Button launching a self-contained secondary window (terminal, viewer, topology map, etc.).
  * **\-Number:** Numeric spin/stepper input (- / \+).
  * **\-Slider:** Paired horizontal slider bar + two-way synchronized numeric input field + unit suffix (`x3d_slider_entry_new()`).
  * **\-InputField / \-Dropdown:** Standard staged inputs.
* **Nested Section Grid Rules:**
  * 3-column nested option grids justified to the scaled page.
  * Rows with fewer than 3 items (-Toggle and \-PopUp) are centered and justified cleanly across normal and narrow widths.
* **Dirty-State Safeguard:** Switching sidebar pages (or closing the window) with unapplied staged changes presents an `AdwAlertDialog` prompting the user to **Save**, **Discard**, or **Cancel** navigation. Selecting **Save** only clears `dirty` and completes navigation if every `"apply"` handler succeeds; if any IPC or sysfs write fails, navigation is aborted and the user remains on the current page so unapplied edits are never silently lost.

## **1\. Dashboard *(6-Slot [2×3] Widget Grid — bottom row of 2 empty widgets)***

* **Title Banner** *(Top)*
* **Daemon Status Widget** *(Current Widget — links to Daemon Settings)*
  * Daemon Status, IPC Status, eBPF Health, Scheduler Mode
* **Hardware Status Widget** *(Current Widget — links to Hardware Modes standalone view; selecting a mode applies, updates status, and closes view; active mode shows active color)*
  * v-Cache Mode, Boost Mode, CCD State, CCD0 State, CCD1 State
* **System Profile Widget** *(Current Widget — links to System Profile standalone view)*
  * Driver Mode, EPP Profile, Dynamic EPP Status *(new)*, Governor, SMT Status, Platform
* **Power & Current Limits Status Widget** *(Current Widget — always displays live telemetry regardless of WMI presence; reports `ONLINE` when `amd-wmi-aod` is hooked and `OFFLINE` otherwise; clicking the card pops out the live AOD Telemetry Visualizer)*
  * PPT, TDC, EDC, Mem Clock, AOD Status

## **2\. CPU**

* **CPU Dashboard Widget** *(Current Widget — live CPU load, per-CCD load, average/peak clocks, `k10temp`/`zenpower` temps, PPT power, active profile; clicking the card pops out the live CPU Telemetry Visualizer)*
* **System Profile Widget** *(Staged Widget — nested section)*
  * Driver Mode \-Dropdown
  * Scaling Governor \-Dropdown
  * EPP Profile \-Dropdown
  * Dynamic EPP (dynamic\_epp) \-Toggle *(new — probe /sys/devices/system/cpu/amd\_pstate/dynamic\_epp or dynamic EPP support; automatically lock/dim the EPP Profile dropdown when enabled)*
  * Precision Boost \-Toggle
  * Per-Core Frequency Floor (amd\_pstate\_floor\_freq) \-PopUp *(new — gate behind sysfs existence check)*
  * **Left-aligned:** Save Profile / Load Profile *(placeholder stub until Phase 6)*
* **Hardware Modes Widget** *(Immediate Widget — nested section; pressing applies immediately and highlights active mode)*
  * Cache \-Toggle
  * Frequency \-Toggle
  * Dual \-Toggle
  * Default \-Toggle
  * Auto/Reset \-Toggle

## **3\. GPU**

* **GPU Dashboard Widget** *(Current Widget — live telemetry stream with GPU selector; clicking the card pops out the live GPU Telemetry Visualizer)*
* **Launcher Buttons** *(Seated directly below GPU Widget; app title only, no subtext)*

## **4\. Thread Settings**

* **Live IRQ / CCD Verification Readout Banner** *(Current Widget — 3-column × 2-row readout showing `IRQ Affinity: Enabled/Disabled` alongside live CCD/core bindings for `GPU`, `NVMe`, `USB`, `NIC`, and `Audio`)*
* **IRQ Vectors Widget** *(Staged Widget — nested section)*
  * **Hardware Affinity Controls:**
    * IRQ Subsystem \-Toggle
    * GPU Interrupts \-Toggle
    * NVMe Storage \-Toggle
    * USB Controllers \-Toggle
    * Network Interfaces \-Toggle
    * Audio Devices \-Toggle
* **Pinning / Affinity Topology Map** *(Staged Widget)*
  * Affinity Level \-Dropdown *(Auto / By Die / Manual)*
  * Topology Pin/Bind Map \-PopUp *(polls/refreshes while window is open)*
    * amd\_pstate\_prefcore\_ranking Readout *(new — live CPPC preferred-core rank per core)*
    * Core Pin Map

## **5\. Daemon Settings**

* **Daemon Behavior Widget** *(Staged Widget — nested section)*
  * Polling Interval \-Slider
  * Load Threshold \-Slider
  * Watch Mode \-Toggle
  * Detection Level \-Dropdown
  * Fallback Profile \-Dropdown
  * State \-Dropdown
  * eBPF Scheduler \-Toggle
  * \*CallToAction\*
* **Lifecycle Modes Widget** *(Immediate Widget — nested section)*
* **Systemd Unit / Service Boot Persistence** \-LiveToggle
* **Advanced Options** \-LiveToggle *(exposes Advanced Configuration, which nests AOD Controls at the top when WMI is supported)*

## **6\. Profile Manager**

* **Process Profile** *(Nested section — placeholder stub until Phase 6)*
* **Games Profile** *(Nested section)*
* **Management** \-PopUp *(placeholder stub until Phase 6)*
  * Backup
  * Save Configuration to Named Profile
  * Restore Configuration from Saved Profile
  * Delete
  * Overwrite

## **7\. Advanced Configuration *(Gated behind Advanced Options)***

* **AOD Controls (AMD-WMI-AOD)** *(Staged Widget — nested at the very top of Advanced Configuration; capability-gated behind `CAP_AOD` / `Force Show Unsupported Options`; enforces dynamic runtime `*_min`/`*_max` firmware guardrails on probe/profile-load/apply, fails closed per knob if a `*_min` or `*_max` file is absent, treats `ppt`/`tdc`/`edc` as write-only without read-back, and opens a modal notice window stating **"Please Restart to Apply Firmware Settings"** upon Apply)*
  * PPT \-Slider *(slider + synced input field, `W`)*
  * TDC \-Slider *(slider + synced input field, `A`)*
  * EDC \-Slider *(slider + synced input field, `A`)*
  * Mem Clock \-Slider *(slider + synced input field, `MHz`)*
  * IRQ / Firmware SMT \-Toggle *(capability-gated behind `CAP_AOD_SMT`)*
* **Load / Polling Hysteresis Logic** *(Staged Widget — nested section; placeholder stub until Phase 6)*
  * Start/Stop \-Number
  * Bearing \-Slider *(slider + synced input field, `%`)*
  * Baselines \-PopUp *(Up-Threshold, Down-Threshold, Cooldown Timer, Sample Window)*
* **Network Settings** *(Staged Widget — nested section)*
  * **Performance & Throughput:**
    * Default QDisc \-InputField
    * TCP Congestion Control \-Dropdown *(new — probe /proc/sys/net/ipv4/tcp\_available\_congestion\_control)*
    * Throughput Buffer Preset \-Dropdown *(new — Stock, 1 Gbps, 2.5+ Gbps; applies matched rmem\_max, wmem\_max, tcp\_rmem, tcp\_wmem, netdev\_max\_backlog, tcp\_max\_syn\_backlog tuple)*
    * TCP Fast Open \-Number
    * TCP Slow Start After Idle \-Toggle *(new — 0 \= off/active, 1 \= stock)*
    * TCP ECN \-Toggle *(new — 1 \= on, 2 \= fallback/default)*
    * 5G / CGNAT Keepalive \-Toggle *(new — bundles tcp\_keepalive\_time=120, intvl=30, probes=3)*
    * NIC Coalescing Hints \-Toggle
  * **Security & Filtering:**
    * Reverse Path Filter \-Number
    * Accept Source Route \-Toggle
  * **Remote Management:**
    * Server IP Address \-InputField
    * Server Port \-InputField
    * SSH Mode \-Dropdown
* **Kernel Settings** *(Staged Widget — nested section)*
  * Sched BORE \-Toggle
  * Burst Penalty Offset \-Number
  * Burst Inheritance \-Toggle
  * CFS Bandwidth Slice (BORE Scheduler Tunables) \-Number
  * VM Max Map Count \-InputField
  * Split-Lock Mitigation \-Toggle
  * NMI Watchdog \-Toggle
  * PCIe ASPM Policy \-Dropdown *(new)*
  * CPU DMA Latency / Deep C-State Guard \-Toggle *(new)*
  * Transparent Huge Pages \-Dropdown
* **Developer Mode** \-LiveToggle *(exposes Developer Options)*

## **8\. Developer Options *(Gated behind Advanced Configuration \+ Developer Mode)***

* **Top Action Row:** Real-Time Observer \-PopUp *(left-aligned)* \| Latest Crash Dumps for CoreDump Analysis \-Button *(right-aligned)*
* **Clang-Tidy Static Analysis Widget** *(Nested section)*
  * Diagnostics \-Toggle, Bugprone \-Toggle, Modernize \-Toggle, Readability \-Toggle, Performance \-Toggle, Portability \-Toggle, Analyzer \-Toggle
  * Analyze and Present Report \-PopUp
* **Cppcheck Bug Hunting Widget** *(Nested section)*
  * All Cppcheck Checks \-Toggle, Warning Messages \-Toggle, Coding Style Checks \-Toggle, Performance Suggestions \-Toggle, Portability Warnings \-Toggle, Informational Messages \-Toggle, Unused Functions and Variables \-Toggle
  * Analyze and Present Report \-PopUp
* **Valgrind Runtime Analysis Widget** *(Nested section)*
  * Analysis Mode \-Dropdown, Leak Kinds \-Dropdown, High CPU Overhead / Origin Tracking \-Toggle
  * Analyze and Present Report \-PopUp
* **Debug Logging** \-LiveToggle
* **Journal Maintenance** *(Nested section)*
  * Archive Session \-PopUp, Manual Rotation \-PopUp
* **Journal Rotation Settings** *(Staged Widget — nested section)*
  * Retention Days \-Number, Max Size \-InputField
  * \*CallToAction\*
* **Audits** *(Nested section)*
  * Timer Measuring Switching Latency \-LiveToggle *(new)*
  * Hardware 'Dry Run' Mode \-PopUp *(new)*
* **UI Overrides Footer Widget** *(Staged Widget)*
  * Force Show Unsupported Options (Disables Discovery Logic) \-Toggle
  * \*CallToAction\*

## **9\. Extras**

* **Interface Theme** *(Staged Widget — nested section)*
  * Main Theme \-Dropdown, Color \-Dropdown, Font \-Dropdown, Size (ptx) \-Number, Widget Update Interval (ms) \-Number *(250–5000 ms live telemetry cadence)*
* **Desktop Widgets & Status Notifier Item** *(Staged Widget — nested section; merged dual-pathway Desktop Widgets + KDE6 SNI)*
  * Pin to Desktop (GTK4 Window) \-Toggle
  * KDE6 Plasma Widget (Plasmoid) \-Toggle
  * KDE6 SNI: Enabled / Disabled \-Toggle
  * Configure Widgets \-PopUp
  * SNI Style \-PopUp
* **Document Viewer** \-PopUp
  * README, Help, Great Book of Insults, Manpage, Architecture, Disclaimer, License, etc
* **About** *(Current Widget — nested section)*
  * Version, Author, Release Year
* **EMERGENCY OVERRIDE Widget** *(Nested section; steady high-contrast danger border; gated behind confirmation prompt before resetting all states to defaults, returning control to drivers, and erasing saved profiles)*
