# ROADMAP.md

## AMD x3D V-Cache Technology Optimizer - v2.0.0

### Toggle Control - Community Edition

#### Code base is pure POSIX C Utiliy

### Refer to `ARCHITECTURE.md`

- Core Libraries for x3D Utility
    * Populate with build/make/install files
    * Provide Mode Setting
    * Set Elevated Permissions/Privileges
    * Create matrix of CLI configurable options that is agnostic to front-ends
    * Game Wrapper
    
- Daemon for automation
    * Polling for Game/LLM loads
    * Game List
    * eBPF for deterministic detection at kernel level
        - Ring buffer for kernel communication
        - `sched_process_Exec` tracepoint probe
        - depends on libbpf and lippbf-dev(el) and clang and llvm
    * Unix Domain Socket
        - IPC Robustness
        - POSIX Signal Handling
    * Hysteresis Logic options
    * Ensure daemon suspends itself untl user resumes daemon if mode manually
        set while daemon running
    * Versioning handshake
    * Allow CLI to "attach" to the daemon and see live events showing CPU load
        changes and real time eBPF triggers `x3d-toggle monitor`
    * Fail safe fallback if daemon errors
    
- Run Valgrind and smoke test/linter/trial by fire 

- Add options for setting CPU Affinity `sched_setaffinity`
- Add options for setting GPU IRQs

- Clang/llvm check, shellcheck

-Graphical UX
    * TUI (nCurses)
    * Kdialog
    * WebUI (GO)
    * KDE6 SNI(SystemNotifierIcon)
    
    
- Documentation
    * Update all documentations
    * README.md and manpage should reflect all features/options available 
      in utility as standard user mode
    * CHANGELOG.md reflects changes from v1.2.0/v1.3.0 to v2.0.0
    * Legal framework is laid down.
    
    
- Add Developer Mode Environment options and documentations relating to 
    DEV_MODE=1
    * logger - stdout format
    * Cpu Spike/Stress test
    * Timer - measure latency
    * Mock game script
    * Hardware `Dry Run` mode

- Update Makefile and PKGBUILD for production release
    * `cppcheck` or `scan-build` in Makefile
    * Move source dir to Git
    * update sha256

**Copyright ©️ 2026 Pyrotiger - License: GPLv3**
