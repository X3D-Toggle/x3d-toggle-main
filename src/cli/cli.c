/* CLI Transport Layer for the X3D Toggle Project
 *
 * `cli.c`
 *
 * Master dispatch table routing user inputs to modular backend controllers.
 */

#include "../../include/cli.h"
#include "../../include/libc.h"

#define _POSIX_C_SOURCE 202405L

const Command cmd_table[] = {
    {"enable", cli_daemon_enable, "Soft Wake: Start daemon (no autostart)"},
    {"start", cli_daemon_start, "Hard Wake: Start daemon and enable autostart"},
    {"wake", cli_daemon_wake,
     "Poke: Refresh state loop via IPC to active status"},
    {"sleep", cli_daemon_sleep,
     "Firm Sleep: Suspend daemon logic loop to sleep status"},
    {"stop", cli_daemon_stop, "Terminate: Kill daemon process gracefully"},
    {"disable", cli_daemon_disable,
     "Unregister: Stop and remove autostart rules"},
    {"elk", cli_daemon_enable, "Alias for enable/default"},
    {"rooster", cli_daemon_wake, "Alias for wake"},
    {"pause", cli_daemon_sleep, "Alias for sleep"},
    {"koala", cli_daemon_sleep, "Alias for sleep"},
    {"loris", cli_daemon_stop, "Alias for stop"},
    {"sloth", cli_daemon_disable, "Alias for disable"},
    {"sonic", cli_daemon_start, "Alias for start"},
    {"cache", cli_mode_cache,
     "Force CCD0: Prioritize 3D V-Cache, suspending daemon"},
    {"frequency", cli_mode_frequency,
     "Force CCD1: Prioritize frequency cores, suspending daemon"},
    {"dual", cli_mode_dual,
     "Unpark all: Maximize throughput across CCDs, suspending daemon"},
    {"swap", cli_mode_swap,
     "Invert Priority: Dynamically flip preferred core tags, suspending "
     "daemon"},
    {"auto", cli_mode_auto,
     "Hard Reset: Restore base CPPC states, removing daemon from autostart and "
     "terminating current session"},
    {"reset", cli_mode_reset,
     "Restore/Reprobe: Revert to autonomous daemon state by reloading current "
     "daemon configuration"},
    {"default", cli_mode_default,
     "Soft Reset: Restore daemon default configuration and restart, but leave "
     "CPPC and v-cache alone"},
    {"rabbit", cli_mode_cache, "Alias for cache"},
    {"cheetah", cli_mode_frequency, "Alias for frequency"},
    {"bear", cli_mode_dual, "Alias for dual"},
    {"chameleon", cli_mode_swap, "Alias for swap"},
    {"moose", cli_mode_default, "Alias for default"},
    {"elephant", cli_mode_reset, "Alias for reset"},
    {"interval", cli_config_interval,
     "Polling Rate: Adjust daemon evaluation loop frequency"},
    {"threshold", cli_config_threshold,
     "Compute Load: Adjust background compute detection threshold"},
    {"fallback", cli_config_fallback,
     "Baseline State: Hardware state with no active triggers"},
    {"detection", cli_config_detection,
     "Detection Mode: Toggle between Polling and eBPF tracking for the task "
     "scheduler"},
    {"polling", cli_config_polling,
     "Detection Mode: Disable eBPF and fall back to polling heuristics for "
     "task scheduler"},
    {"ebpf", cli_config_ebpf,
     "Detection Mode: Enable eBPF tracker for Zero-Latency task scheduler"},
    {"server", cli_config_server,
     "Dashboard Server: Set dashboard URL and Port"},
    {"add", cli_config_add, "Watch: Append executable to trigger list"},
    {"remove", cli_config_remove, "Ignore: Strip executable from trigger list"},
    {"list", cli_config_list, "View: Output active monitored processes"},
    {"profile", cli_config_profile,
     "Macro Management: Snapshot/load states to JSON/YAML"},
    {"sync", cli_config_sync,
     "Persist: Flush session config to persistent storage"},
    {"update", cli_config_update,
     "Rebuild: Regenerate /etc/x3d-toggle.d from edited settings.conf / "
     "games.conf"},
    {"tick", cli_config_interval, "Alias for interval"},
    {"anchor", cli_config_fallback, "Alias for fallback"},
    {"commit", cli_config_sync, "Alias for sync"},
    {"bpf", cli_config_ebpf, "Alias for ebpf"},
    {"framework", cli_framework,
     "Orchestration: Access structural scaffolding and installation suite"},
    {"--gen-config", cli_config_generate,
     "Hidden: Generate C-native topology configuration"},
    {"dialog", cli_status_dialog, "Dynamic Monitor: Pop-out status window"},
    {"monitor", cli_status_dialog,
     "Internal Loop: Refresh view, alias for dialog"},
    {"owl", cli_status_owl,
     "Self-Contained Dashboard: Conditionally start daemon & monitor"},
    {"linter", cli_diag_linter,
     "Developer Audit: Project-wide static and dynamic analysis scan"},
    {"debug", cli_diag_debug,
     "MVC Observer: Real-time daemon and eBPF event monitor"},
    {"status", cli_diag_status,
     "Echo Snapshot: Daemon state and IPC connections"},
    {"rotate-log", cli_diag_rotate,
     "Clean: Truncate and compress standard logs"},
    {"rotate-audit", cli_diag_rotate,
     "Clean: Truncate and compress audit records"},
    {"rotate-all", cli_diag_rotate,
     "Clean: Truncate and compress all project logs"},
    {"journal-log", cli_diag_journal,
     "Journal: Manage project activity logs"},
    {"journal-audit", cli_diag_journal,
     "Journal: Manage diagnostic audits"},
    {"journal-coredump", cli_diag_journal,
     "Journal: Manage service coredumps"},
    {"journal-all", cli_diag_journal,
     "Journal: Manage all project logs"},
    {"journal-archive", cli_diag_archive,
     "Archive: Mandatory session zipping (.gz)"},
    {"trim", cli_diag_rotate,
     "Trim: Cleanup and retention management"},
    {"journal-dump", cli_diag_dump,
     "Journal: Analyze service coredumps via coredumpctl"},
    {"hawk", cli_diag_status, "Alias for status"},
    {"perf", cli_cppc_perf, "Set EPP via IPC: Maximum Performance (0x0)"},
    {"bal", cli_cppc_bal, "Set EPP via IPC: Balanced Performance"},
    {"eco", cli_cppc_eco, "Set EPP via IPC: Balanced Power"},
    {"power", cli_cppc_power, "Set EPP via IPC: Minimum Power (0xff)"},
    {"epp", cli_cppc_epp, "Set EPP_CCD via IPC: Shift focus [CCD] [VAL]"},
    {"cppc-raw", cli_cppc_raw, "Set RAW via IPC: Direct register write [VAL] [DEV]"},
    {"cppc-mode", cli_cppc_mode, "Set PSTATE via IPC: Shift operational mode"},
    {"cppc-target", cli_cppc_target, "Set CPPC via IPC: Apply desired performance target"},
    {"governor", cli_cppc_governor, "Set GOVERNOR via IPC: Shift frequency scaling policy"},
    {"prefcore", cli_cppc_prefcore, "Set PREFCORE via IPC: Toggle highest_perf hints"},
    {"fallback", cli_misc_fallback, "Baseline State: Hardware state with no active triggers"},
    {"stress-cpu", cli_stress_cpu, "Compute Load: Generate mock throughput load for validation"},
    {"insults", cli_misc_insults,
     "The Great Book of Insults: View all funny failure messages"},
    {"bible", cli_misc_insults, "Alias for insults"},

    {NULL, NULL, NULL}};

const Command *find_command(const char *name) {
  if (!name)
    return NULL;
  for (int i = 0; cmd_table[i].name != NULL; i++) {
    if (strcmp(cmd_table[i].name, name) == 0) {
      return &cmd_table[i];
    }
  }
  return NULL;
}

/* end of CLI.C */
