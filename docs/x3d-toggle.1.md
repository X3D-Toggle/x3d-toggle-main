# NAME
x3d-toggle - AMD X3D V-Cache Technology Optimizer CLI

# SYNOPSIS
x3d-toggle
x3d-toggle [COMMAND | MODE | ALIAS] [ARGS...]

# DESCRIPTION
x3d-toggle is the frontend for the AMD X3D V-Cache Technology Optimizer (v2.0.0). Invoking `x3d-toggle` with no arguments launches the GTK4 desktop dashboard (`x3d-gui`); passing a subcommand interacts directly with the POSIX C backend and IPC daemon.

Supports asymmetric dual-CCD processors (Ryzen 9 9950X3D, 9900X3D, 7950X3D, 7900X3D), symmetric Dual 3D V-Cache processors (Ryzen 9 9950X3D2), and single-CCD X3D processors (Ryzen 7 9850X3D, 9800X3D, 7800X3D) across Linux kernels 6.13 through 7.3+.

This CLI is designed following a strict Model-View-Controller (MVC) architecture. It acts as an agnostic presentation layer, meaning other frontends (XUI, WebUI, YAD) can easily wrap these commands without needing to replicate hardware-level logic.

# MODES (STATE CONTROL)
The following arguments directly alter the hardware state or daemon behavior. Passing a manual mode will automatically suspend the x3d-daemon to prevent race conditions.

### auto
Hard Reset: Restores native CPPC states, removes any persistent autostart rules, and terminates the current daemon session.

### cache
Force CCD0 (Rabbit 🐇): Prioritizes the CCD with the 3D V-Cache to minimize latency and maximize gaming performance.

### frequency
Force CCD1 (Cheetah 🐆): Prioritizes the CCD with higher clock speeds for raw compute and background tasks.

### dual
Unpark All (Bear 🐻): Maximizes throughput across both CCDs simultaneously.

### swap
Invert Priority (Chameleon 🦎): Dynamically flips preferred core tags.

### default
Soft Reset (Moose 🫎): Restores the default daemon configuration (polling rate, load thresholds) but leaves hardware states and asymmetric isolation untouched.

### reset
Restore/Reprobe (Elephant 🐘): Syncs the current configuration with the hardware state and resumes/reloads the autonomous state loop.

# DAEMON CONTROLS
### start
Hard Wake (Sonic 🦔): Registers the daemon for persistent autostart and initializes the management session.

### enable
Soft Wake (Elk 🫎): Starts the daemon for the current session only without modifying persistent autostart registration.

### wake
Poke (Rooster 🐓): Refreshes the internal state loop via IPC.

### sleep
Firm Sleep (Koala 🐨): Safely suspends the daemon logic loop.

### stop
Terminate (Loris 🐒): Gracefully kills the daemon process.

### disable
Unregister (Sloth 🦥): Stops the daemon and removes all persistent autostart rules.

# CONFIGURATION COMMANDS
### interval <ms>
Polling Rate (Tick ⏱️): Adjust the daemon evaluation loop frequency.

### threshold <%>
Compute Load: Adjust the background compute detection threshold.

### fallback <mode>
Baseline State (Anchor ⚓): Hardware state used when no triggers are active.

### detection
Detection Mode: Toggle between Polling and eBPF tracking.

### ebpf
BPF Enable: Re-enable zero-latency eBPF tracker (Alias: bpf).

### polling
Polling Mode: Disable eBPF and fall back to polling heuristics.

### sync
Persist (Commit 💾): Flush the current session configuration to storage.

### profile
Macro Management: Snapshot/load states to JSON/YAML.

### add <executable>
Watch: Append an executable to the detection trigger list.

### remove <executable>
Ignore: Strip an executable from the detection trigger list.

### list
View: Output the active monitored processes.

### server url:port
Connects to local daemon via server socket (default: localhost:2997)  

# DIAGNOSTICS
### status
Echo Snapshot (Hawk 🦅): Outputs current daemon state, IPC connections, and active hardware topology.

### insults
The Great Book of Insults (Bible 📖): View all funny CLI failure messages.

### -h, --help
Show the standard help message and command syntax.

# ALIASES
For ease of use (and fun), the thematic aliases map to standard commands as follows:

* sonic      -> start
* elk        -> enable
* rooster    -> wake
* pause      -> sleep
* koala      -> sleep
* loris      -> stop
* sloth      -> disable
* rabbit     -> cache
* cheetah    -> frequency
* bear       -> dual
* chameleon  -> swap
* moose      -> default
* elephant   -> reset
* tick       -> interval
* anchor     -> fallback
* commit     -> sync
* bpf        -> ebpf
* hawk       -> status
* bible      -> insults

# XUI ICONOGRAPHY
x3d-toggle uses a standardized symbol system (XUI) to report states:

* 🐇  Rabbit    -> Cache Optimized (CCD0)
* 🐆  Cheetah   -> Frequency Optimized (CCD1)
* ♊  Dualize   -> All CCDs Active
* 🦅  Boost     -> Turbo/Boost Enabled
* ✅  Alright   -> Success / Active
* ❌  X-Out     -> Error / Disabled
* ⚠️  Warn      -> Potential Issue

# GUI INTEGRATION / API MAPPING
x3d-toggle is designed to be easily wrapped by custom graphical user interfaces (GUIs). Frontend developers should use standard system calls (e.g., exec() or popen()) to invoke the CLI.

1. Reading State (For UI initialization and polling):
   Command: `x3d-toggle status`
   Returns a standard string parsable for active modes and daemon state.

2. Setting Manual State:
   Command: `x3d-toggle cache` or `x3d-toggle frequency`
   Action: Instructs the backend to safely suspend automation and write to the sysfs node via the IPC socket.

Exit Codes:
   0 : Success
   1 : General failure (Invalid argument or Permission denied)

# FILES
### /etc/x3d-settings.conf
Main system-wide default configuration file for daemon heuristics.

### /etc/x3d-toggle.d/
User configuration directory containing persistent overrides.

### /etc/x3d-toggle.d/x3d-mygames.list
User-defined game list for daemon detection. Safe to edit. Will not be overwritten by package updates.

### /usr/share/x3d-toggle/x3d-games.list
System-wide default game list shipped with the package.

### /sys/devices/platform/AMDI*/amd_x3d_mode
The kernel sysfs node accessed by the backend.

# AUTHOR
Pyrotiger (Copyright © 2026)
Released under the GPLv3 License.

# SEE ALSO
systemctl(1), udev(7)
