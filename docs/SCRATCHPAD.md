# Scratchpad for brainstorming, debugging notes, etc


### ISSUE: CPU OVERHEAD is noisy

  * FIX: Filter by UID and comm before touching environ or maps: Use fstat() on /proc/<pid> (or check st_uid) and immediately skip UID < 1000 (root and system daemons). That eliminates 80%+ of those 768 processes in a single syscall. Only open /proc/<pid>/maps if comm or environ already matches a candidate (e.g., wine*, *.exe, SteamAppId=, PROTON_).
    * Verification: Re-run the strace path breakdown; /proc/<N>/maps count should drop from 748 to single digits.

  * FIX: Cache inspected PIDs so each process is scanned once: A process's environ is fixed at execve and doesn't need to be re-read every 3 seconds. Keep a small table of (pid, start_time) pairs (or a fixed-size hash map) for already-classified PIDs and skip them on subsequent polls until they exit.
    * Verification: Over a 6-second strace -c window, total open() and read() calls should drop from ~88,000 to a few hundred.

  * FIX: GATE BEHIND BPF_ACTIVE: The gate flag state: Verify bpf_active (or your equivalent flag) is actually set to true after handle_exec/handle_exit attach, and isn't getting reset or shadowed in a separate translation unit.

### ISSUE CHECK: Secondary /proc callers: Check if another routine in the 3-second tick—like a Wine/Proton detector, active-game verifier, or an IPC status handler—is calling the full /proc sweep independently of the main eBPF gate.

### ISSUE CHECK: Startup-only vs. loop placement: Make sure the initial /proc scan (to catch games already running before the daemon started) runs once before the main loop rather than inside it.

### ISSUE: Global button override in Imperial & LACT: Both theme-imperial.css and theme-lact.css define a bare button { ... } rule (#142410 and #2f3439), which overrides Libadwaita's built-in AdwAlertDialog button classes (suggested / destructive). Because your primary/destructive styles are scoped to button.x3d-primary and button.x3d-destructive, standard GTK/Adwaita dialogs fall back to the bare button rule unless you also target button.suggested-action and button.destructive-action (or button.suggested / button.destructive for AdwAlertDialog).
  * FIX: Standardize styles across buttons

 ### ISSUE: Obsidian (theme.css) widget parity: theme.css omits dropdown>button, popover>contents, textview, and scrollbar rules that you already styled in theme-imperial.css (and mostly in theme-lact.css), so dropdown popovers and log views will fall back to default Adwaita dark surfaces in Obsidian.
  * FIX: standardize style for popdown in Obsidian

 ### ISSUE: Font fallback in Imperial: theme-imperial.css relies on "Inter", "Cinzel", and "Share Tech Mono"—if a Chaotic-AUR user doesn't have those fonts installed, GTK will silently fall back to system sans/serif/monospace, which is totally fine, though adding ttf-cinzel or inter-font to optdepends (or bundling/noting them) is a nice touch if you want the exact look out of the box. 
  * FIX: Point to new folder as fallback option: '../assets/Cinzel_Inter_Share_Tech_Mono'

### ISSUE: PPT live telemetry on main dashboard shows null value
  * FIX: Plug same telemetry value from AOD widget which shows live value

## Ensure Configuration option changes are persistent after exiting any UI.

### X3D Toggle CLI Paradigm

### This document outlines the complete command-line interface paradigm for the X3D Toggle project, integrating daemon lifecycle management, topology control, amd_pstate scaling heuristics, eBPF scheduler overrides, and diagnostic telemetry

### CLI Paradigm

* Daemon Control - Lifecycle and State Control Management
  * `enable`/`start`/`elk` - Hard Wake: Initializes the daemon, actively managing topology and hardware P-states in the current session but does not set autostart.
  * `wake`/`restart`/`rooster` - Soft Wake: Pokes the daemon via IPC to refresh its state loop or brings it out of sleep, without altering autostart configurations.
  * `sleep`/`suspend`/`koala` -  Firm Sleep: Delegates(suppresses) daemon to background to allow for manual commands to be passed without overriding.
  * `stop`/`loris` - Terminate: Kills daemon process gracefully but leaves topology and hardware P-states as is.
  * `disable`/`sloth` - Unregister: Executes `stop` and removes autostart symlinks, but leaves topology and hardware P-states as is.
  * `status` - Shows status: Displays status of the daemon state

* Manual Mode Control - Topology and Thread Management
  * `cache`/`rabbit` - Force CCD0: Executes `sleep` and parks the secondary node and forces the scheduler to prioritize the primary latency-optimized node. Ideal for latency-sensitive game threads.
  * `frequency`/`cheetah` - Force CCD1: Executes `sleep` and parks the v-Cache CCD and forces the scheduler to the higher-clocking frequency CCD.
  * `dual`/`bear` — Unpark All: Wakes all cores across both CCDs, removing explicit topology limits for heavy compute workloads, maximizing frequency bandwidth across the X3D spectrum.
  * `swap`/`chameleon` — Invert Priority: Dynamically toggles the preferred core flags between CCD0 and CCD1 without parking either, using CPPC tags to guide the scheduler.
  * `park`/`hibernator` [ccd0|ccd1] — Manual Offline: Explicitly offlines (hotplugs) a specific CCD via /sys/devices/system/cpu/cpuX/online for absolute thermal/power isolation.
  * `pin`/`spider` [process_name] [ccd0|ccd1|dual] — Affinity Routing: Uses cgroups v2 to soft-pin a specific process to a designated CCD without offlining/parking the other. Crucial for simultaneous gaming (CCD0) and LLM/streaming (CCD1) workloads.
  * `isolate`/`husky` [ccd0|ccd1|off] — Background Sweeper: Uses `cpuset`/`tasksets` to actively migrate all non-essential user-space OS background threads to the opposite CCD. Guarantees zero thread pollution on the gaming CCD.
  * `priority`/`mantis` [process_name] [normal|high|realtime] — Scheduler Policy: Uses `chrt` to elevate a specific process to SCHED_FIFO (real-time). Prevents the Linux scheduler from preempting the game loop for any reason.
  * `weight`/`ant` [process_name] [1-10000] — Proportional Allocation: Adjusts the cpu.weight in cgroups v2. Prioritizes CPU time proportionally among competing processes without applying strict hard-pins, preventing total background starvation.
  * `bpf-sched`/`octopus` [default|lavd|rusty] — eBPF Scheduler Override: Attaches a custom sched_ext (SCX) BPF scheduler to override standard CFS/EEVDF behavior, natively leveraging scx_bpf_select_cpu_dfl for advanced asymmetric topological routing.

* CPPC/Pstate Mode Control - AMD Pstate/EPP scaling heuristics Management and Defaulting Options
  * `auto`/`elk` - Initial Daemon orchestration: `enable` and `default` and symlinks autostart, letting it automate the orchestration of CPPC/v-Cache/Pstates according to session state in a (re)freshed daemon state
  * `perf`/`panther` — Max EPP: Directly writes performance to the energy_performance_preference sysfs node, pinning clocks high.
  * `bal-perf`/`fox` — Balanced Perf EPP: Writes balance_performance to sysfs. Ideal for mixed streaming/gaming workloads where responsiveness is needed without constant thermal saturation.
  * `bal-power`/`badger` — Balanced Power EPP: Writes balance_power to sysfs. Ideal for background LLM processing, yielding high throughput with aggressive energy efficiency.
  * `power`/`turtle` — Min EPP: Directly writes power to the sysfs node, aggressive downclocking for idle states.
  * `epp-target`/`cobra` [ccd0|ccd1] [perf|bal-perf|bal-power|power] — Asymmetrical EPP: Applies specific EPP heuristics to a targeted CCD (e.g., pinning CCD0 for gaming while starving CCD1 to lower package temps).
  * `epp-raw`/`viper` [0-255] [ccd0|ccd1|all] — Granular EPP: Writes a raw byte value to the EPP sysfs node for exact heuristic tuning, bypassing the 4 standard text strings.
  * `governor`/`wolf` [performance|powersave|schedutil] — CPUFreq Governor: Shifts the active scaling governor. Critical when operating amd_pstate in guided or passive modes where the governor dictates frequency scaling limits.
  * `prefcore`/`lynx` [on|off] — Preferred Core Toggle: Writes to /sys/devices/system/cpu/amd_pstate/prefcore. Instructs the Linux scheduler to respect or ignore the hardware's ACPI CPPC highest_perf tags (crucial for verifying if Garuda is prioritizing the Frequency CCD correctly).
  * `tdp`/`rhino` [watts|default] — RAPL Power Limit: Writes to /sys/class/powercap/intel-rapl/intel-rapl:0/power_limit_uw to hard-cap socket wattage. Prevents CCD1 thermal spillover into CCD0 during max-load dual workloads.
  * `platform`/`orangutan` [performance|balanced|low-power] — ACPI Platform Profile: Toggles /sys/firmware/acpi/platform_profile to align the motherboard's underlying SMU base target with the daemon's CPPC goals.
  * `cstate`/`walrus` [on|off] — Deep Sleep Toggle: Writes to /dev/cpu_dma_latency to set max_cstate=0. Prevents parked or idle cores from entering C6 sleep states, completely eliminating the 1-2ms hardware wake penalty during bursty workloads.
  * `fabric`/`gorilla` [auto|max] — FCLK State Clamp: Pings the AMD Data Fabric power management sysfs. Crucial for ensuring your FCLK does not aggressively downclock during transient I/O lulls.
  * `default`/`moose` - Hardware Defaults: Restore CPPC/v-Cache/Pstate(X3D) default states but leaves the daemon's execution state alone
  * `reset`/`elephant` - Nuke: `disable` and `default` combined (returns X3D to native (DE) desktop environment control)
  * `mode` [active|guided|passive] — P-State Driver Mode: Shifts the underlying amd_pstate operational mode (e.g., from EPP hints to autonomous range-bounding).
  * `boost`/`falcon` [on|off] — Core Performance Boost: Toggles /sys/devices/system/cpu/cpufreq/boost. Disabling it forces base clocks, excellent for thermal isolation or strict baseline benching.
  * `limit-max`/`eagle` [freq_mhz] — Max Frequency Clamp: Writes to scaling_max_freq to hard-cap clocks (useful for capping the frequency CCD from sapping power budget from the v-cache CCD).
  * `limit-min`/`tortoise` [freq_mhz] — Min Frequency Clamp: Writes to scaling_min_freq to establish a strict performance floor.
  * `uclamp`/`python` [process_name] [min|max] [0-1024] — Utilization Clamping: Writes to cpu.uclamp.min and cpu.uclamp.max. Artificially boosts or caps the utilization signal of a process to force the Energy Aware Scheduler (EAS) to place it on a higher or lower performance tier, bypassing the need for explicit node pinning.
  * `bpf-stats`/`jellyfish`: Dumps the active eBPF map statistics from your attached sched_ext (SCX) scheduler to verify internal queue routing.
  * `cppc-target`/`bull`: Restored from the previous version to ensure the passive mode governor has its required Desired Performance register input.

* Configuration/Ruleset Management - Game List Management
  * `interval`/`tick` [ms] — Polling Rate: Adjusts the daemon's background evaluation loop (e.g., 2000ms). Lower values react faster to launched games; higher values save background CPU cycles.
  * `fallback`/`anchor` [profile_name|default] — Baseline State: Defines the default profile or hardware state the daemon safely reverts to when no monitored process is detected.
  * `add` [rule|game] [process_name] — Watch: Appends a target executable to the daemon's active trigger list (e.g., automatically shifting to rabbit when process_name is detected).
  * `remove`[rule|game] [process_name] — Ignore: Strips the executable from the active trigger list.
  * `list` [rules|games] — View: Outputs the current array of monitored processes and their associated daemon behaviors.
  * `profile` [add|save|load|delete] [profile_name] — Macro Management: Snapshots current Topology, EPP strings, limits, and Boost states into a named YAML/JSON profile (e.g., llm-eco, hardcore-gaming) for rapid switching.
  * `sync`/`commit` — Persist: Flushes the current session's runtime configuration (rules, preferences) to the persistent config file to ensure survival across reboots.
  * `irq`/`shield` [node0|node1|off] — IRQ Affinity Masking: Modifies `/proc/irq/default_smp_affinity` and suspends the native `irqbalance` service. Pinning IRQs to Node1 ensures GPU/Network hardware interrupts do not preempt latency-sensitive threads running on Node0.
  * `smt`/`twin` [on|off] — SMT Toggle: Writes to `/sys/devices/system/cpu/smt/control`. Disabling logical threads can eliminate cache thrashing and improve frame pacing in specific unoptimized engines.
  * `numa`/`ox` [on|off] — Auto-NUMA Balancing: Toggles `/proc/sys/kernel/numa_balancing`. Prevents the Linux kernel from unnecessarily attempting to migrate memory pages between processor nodes on a uniform memory architecture.
  * `3-slice`/`beaver` [process_name] [percentage] — Cache Partitioning: Utilizes hardware Resource Director Technology (RDT/PQoS) to cap the maximum L3 cache allocation for a specific compute/background task, protecting foreground hit-rates.
  * `throttle`/`snail` [process] [percentage]: While weight handles proportional CFS distribution, you need a hard bandwidth ceiling via cgroups v2 (cpu.max) to strictly limit background LLMs from saturating the interconnect/memory bandwidth and starving the gaming CCD.

* Logging & Diagnostics - IPC Command and Status Verifications
  * `status`/`hawk` — Echo Snapshot: Returns a succinct stdout block showing daemon state, active IPC connections, current EPP string, and which CCD is currently preferred/parked.
  * `monitor`/`owl` — Live Feed: Initiates a continuous feed detailing live CPPC state transitions and active frequency averages across CCDs in a clean and concise CLI output.
  * `cppc-perf`/`gauge` — Delivered Performance Metrics: Reads feedback_ctrs dynamically over an interval to calculate exact delivered vs. reference performance.
  * `trace`/`bloodhound` — Kernel Hooking: Temporarily enables cpu_frequency and amd_pstate_perf kernel tracepoints to diagnose OS-to-hardware request latencies.
  * `locate`/`hound` [process_name|PID] — Thread Verification: Queries the kernel/cgroups to return exactly which logical threads (and thus which CCD) a specific process is actively executing on in real-time. Crucial for verifying that your pin or bpf-sched routing is functioning correctly.
  * `validate`/`meerkat` [profile|command] — Dry Run: Parses the command, ruleset, or profile for syntax and hardware compatibility without committing the actual sysfs/cgroup writes. Prevents executing a broken YAML/JSON profile that could crash the daemon.
  * `version`/`info` — Topology & Build: Outputs the current daemon version, build number, and the detected system topology at initialization
  * `help`/`h`/`--help` — Global Reference: Standard output of all available commands.
  * `sensors`/`lizard` - Direct hwmon parsing: a quick CLI readout of Package Power, vSOC, and per-CCD temps is critical for tuning verifications.

* `dev` /[-h]/[help]/[--help] - Echoes all available `dev` CLI commands
  * [enable] - Developer Environment Mode: Changes the flag `DEV_MODE=0` to `DEV_MODE=1`, enabling developer environment mode (Option will be the only developer mode option available in standard user environment)
  * [disable] - Executes `uninstall-dev.sh` and changes the flag `DEV_MODE=1` back to `DEV_MODE=0`. Purging the developer environment and disabling it from reloading.
  * [bug] - Debugging Mode: Enables debugging mode by executing `x3d-debug.sh`
  * [log] - Diagnostics: Initiates verbose logging of every event/trigger/comand/status (injects flag to force entire package to be as verbose as possible in CLI). Appending `off` stops the logging
  * [log-rotate] - Truncates Log: Enabling logging rotation feature ensures that logging will not bloat the package over time.
  * [cpu-stress llm] - CPU Stress Test: Executes `cpu-stress.c` to spike load the CPU mimicking a heavy/full compute load
  * [cpu-stress game] - CPU Stress Test: Executes `cpu-stress.c` to spike load the CPU mimicking a gaming session.
