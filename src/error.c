/* Error Handling Logic for the X3D Toggle Project
 *
 * `error.c`
 *
 * Handles all stdout, stderr and logging routing logic.
 * Also implements an error handling dispatch table for diagnostics. 
 */

#include "xui.h"
#include "cppc.h" // IWYU pragma: keep
#include "error.h"
#include "libc.h"
#include "systemd.h"

#ifndef VAR_LOGS
#define VAR_LOGS "/var/log/x3d-toggle/logs"
#endif

static char buffer_deferred[4096] = {0};
static int buffer_active = 0;

typedef struct {
  const char *summary;
  const char *details;
} msg_t;

static const msg_t registry_msg[] = {
    [0] = {"Success", "Operation completed successfully"},
    [1] = {"Target Sysfs node or file not found",
           "Path missing or inaccessible: %s"},
    [2] = {"Permission denied", "Access to hardware restricted: %s"},
    [3] = {"I/O failure during hardware write", "Sysfs write failed for %s"},
    [4] = {"Daemon communication failure",
           "Failed to contact daemon (Code: %d)"},
    [5] = {"Hardware transition rejected",
           "SMU rejected state change (Code: %d)"},
    [6] = {"Invalid command syntax", "Syntax error: %s"},
    [7] = {"Polkit elevation failed", "Privilege escalation denied"},
    [8] = {"Internal daemon state collision", "Daemon error: %s"},
    [9] = {"Unknown command or process", "%s"},
    [10] = {"System state anomaly",
            "Daemon offline. Utilizing Polkit VIP pass..."},
    [11] = {"System state anomaly", "Hint: Shell session lacks 'x3d-toggle' "
                                    "group. Try full logout or 'sudo'."},
    [12] = {"System state anomaly",
            "Note: Only functional when amd_pstate is in 'passive' mode."},
    [13] = {"Invalid command syntax",
            "Usage: x3d profile [add|save|load|delete] [profile_name]"},
    [14] = {"System state anomaly",
            "Hint: Daemon is offline or socket missing at %s"},
    [15] = {"I/O failure during local write",
            "Error opening local game list: %s"},
    [16] = {"I/O failure during local write",
            "Remove-game for '%s' not yet implemented."},
    [17] = {"Daemon communication failure",
            "Failed to retrieve daemon status."},
    [18] = {"Invalid command syntax", "Usage: x3d cppc [auto|high|normal|low]"},
    [19] = {"Invalid command syntax",
            "Usage: x3d [epp-target|epp-raw] [ARGS...]"},
    [20] = {"Invalid command syntax",
            "Usage: x3d governor [performance|powersave|schedutil]"},
    [21] = {"Invalid command syntax",
            "Usage: x3d [boost|park|unpark|tdp|smt|plat] [ARGS...]"},
    [22] = {"IPC Socket Failure", "Failed to create AF_UNIX socket: %s"},
    [23] = {"IPC Bind Failure", "Failed to bind IPC socket to %s: %s"},
    [24] = {"IPC Ownership Failure",
            "Failed to chown socket to x3d-toggle group: %s"},
    [25] = {"IPC Permission Failure",
            "Failed to enforce 0660 permissions on %s: %s"},
    [26] = {"IPC Listen Failure", "Failed to listen on IPC socket: %s"},
    [27] = {"BPF Load Failure", "Failed to load eBPF object: %s"},
    [28] = {"Linter failure",
            "Automated audit suite reported errors (Code: %d)"},
    [29] = {"Memory allocation failure", "Failed to allocate %zu bytes"},
    [30] = {"Critical Error: Configuration File Not Found",
            "/etc/x3d-toggle.d user configuration file is missing. Run install "
            "or regenerate it."},
    [31] = {"Scaffolding Component Failure", "Failed to generate: %s"},
    [32] = {"Build Artifact Permission Overlap", "Permission denied on: %s. Run 'sudo make clean'."},
    [33] = {"OBLITERATION ABORTED: Self-Destruction Guard Triggered", "Safety Check failed: You are currently working inside the directory you are trying to delete. Path: %s"},
    [34] = {"Cgroup Resource Isolation Failed", "Ephemeral core-set migration rejected (Locked or restricted): %s"},
    [35] = {"Affinity Migration Disruption", "Universal Protocol failed to migrate process affinity (Code: %d)"}
};

static const msg_t registry_status[] = {
    [0] = {"Success", "Operation successful"},
    [1] = {"Hardware Defaults", "Native heuristics restored via %s"},
    [2] = {"Configuration Synced", "Runtime configuration flushed to disk"},
    [3] = {"Process Watchlist Updated", "Added '%s' to %s"},
    [4] = {"Profile Action Successful", "%s"},
    [5] = {"EPP Heuristic Applied", "EPP set to: %s"},
    [6] = {"P-State Mode Shifted", "amd_pstate operational mode set to: %s"},
    [7] = {"CPPC Target Applied",
           "Desired Performance Target register set to: %s"},
    [8] = {"Governor Shifted", "CPUFreq scaling governor set to: %s"},
    [9] = {"Preferred Cores Toggled", "CPPC highest_perf hints toggled: %s"},
    [10] = {"Boost State Toggled", "Precision Boost override: %s"},
    [11] = {"Daemon Initialization", "X3D Toggle Daemon started successfully."},
    [12] = {"BPF Game Detection", "Process '%s' (PID %d) %s. Active: %d"},
    [13] = {"State Transition", "Target: %s | Sysfs: %s | Load: %.1f%%"},
    [14] = {"IPC Server Ready",
            "IPC server bound to %s with strict 0660 permissions."},
    [15] = {"Process Watchlist Updated", "Removed '%s' from %s"},
    [16] = {
        "Dialog Already Active",
        "Status monitor is already open. Close the existing window first."},
    [17] = {"Developer Sync", "UI Framework and build artifacts successfully re-scaffolded."}};

void journal_syslog(int priority, const char *summary, const char *ctx) {
  int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0) return;

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  scat(addr.sun_path, "/run/systemd/journal/socket", sizeof(addr.sun_path));

  char buf[2048];
  int len = printf_sn(buf, sizeof(buf),
    "PRIORITY=%d\n"
    "SYSLOG_IDENTIFIER=x3d-toggle\n"
    "MESSAGE=%s: %s\n",
    priority, summary, ctx);

  sendto(fd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
  close(fd);
}

void journal_file(const char *level, const char *summary, const char *ctx) {
  static char log_path[256] = {0};
  struct timespec ts;
  clock_gettime(0, &ts);

  if (log_path[0] == 0) {
    printf_sn(log_path, sizeof(log_path), 
              VAR_LOGS "/x3d-%zu.log", 
              (size_t)ts.tv_sec);
  }

  char timestamp[32];
  printf_sn(timestamp, sizeof(timestamp), "%zu", (size_t)ts.tv_sec);

  int pri = LOG_INFO;
  if (strcmp(level, "ERR") == 0) pri = LOG_ERR;
  else if (strcmp(level, "WRN") == 0) pri = LOG_WARNING;
  else if (strcmp(level, "FTL") == 0) pri = LOG_CRIT;
  else if (strcmp(level, "DBG") == 0) pri = LOG_INFO;

  journal_syslog(pri, summary, ctx);

  char buf[2048];
  int len = printf_sn(buf, sizeof(buf), "[%s] [%s] %s: %s\n", timestamp, level, summary, ctx);

  int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0664);
  if (fd >= 0) {
    write(fd, buf, len);
    close(fd);
  }
}

static msg_t journal_get(error_code code) {
  int val = (int)code;
  if (val > 0 && val <= 17)
    return registry_status[val];
  val = (val < 0) ? -val : val;
  if (val >= 0 && val <= 35)
    return registry_msg[val];
  return (msg_t){"Unknown Condition", "Unknown code generated"};
}

const char *journal_string(int code) {
  return journal_get((error_code)code).summary;
}

void journal_error(error_code code, ...) {
  msg_t msg = journal_get(code);
  char ctx[1024];
  va_list args;
  va_start(args, code);
  printf_vsn(ctx, sizeof(ctx), msg.details, args);
  va_end(args);

  printf_step("2,${COLOR_RED}${XOUT} Error: ${COLOR_RESET}%s. [X3D-%d]",
              msg.summary,
              (int)code < 0 ? -(int)code : (int)code);
  printf_step("2,${COLOR_RED}Context: ${COLOR_RESET}%s", ctx);

  journal_file("ERR", msg.summary, ctx);

  if ((int)code <= -30) {
    daemon_failsafe(-1);
  }
}

void journal_warn(error_code code, ...) {
  msg_t msg = journal_get(code);

  char ctx[1024];
  va_list args;
  va_start(args, code);
  printf_vsn(ctx, sizeof(ctx), msg.details, args);
  va_end(args);

  if (buffer_active) {
    char tmp[2048];
    printf_sn(tmp, sizeof(tmp), "        ${COLOR_YELLOW}${WARN} [X3D-%d] %s: ${COLOR_RESET}%s\n",
             (int)code < 0 ? -(int)code : (int)code, msg.summary, ctx);
    scat(buffer_deferred, tmp, sizeof(buffer_deferred));
  } else {
    printf_step("2,${COLOR_YELLOW}${WARN} Warning: ${COLOR_RESET}%s. [X3D-%d]",
                msg.summary,
                (int)code < 0 ? -(int)code : (int)code);
    printf_step("2,${COLOR_YELLOW}Context: ${COLOR_RESET}%s", ctx);
  }

  journal_file("WRN", msg.summary, ctx);
}

void journal_info(error_code code, ...) {
  msg_t msg = journal_get(code);

  char ctx[1024];
  va_list args;
  va_start(args, code);
  printf_vsn(ctx, sizeof(ctx), msg.details, args);
   va_end(args);

  printf_step("2,${COLOR_GREEN}${ALRIGHT} ${COLOR_RESET}%s", ctx);
}

void buffer_set(void) {
  buffer_active = 1;
  memset(buffer_deferred, 0, sizeof(buffer_deferred));
}

void buffer_flush(void) {
  buffer_active = 0;
  if (buffer_deferred[0] != '\0') {
    printf_br();
    printf_divider();
    printf_step("2,${COLOR_YELLOW}Anomaly Journal:${COLOR_RESET}");
    printf_string("%s", buffer_deferred);
    memset(buffer_deferred, 0, sizeof(buffer_deferred));
  }
}

void journal_write(int priority, const char *format, ...) {
  va_list args;
  va_start(args, format);
  char ctx[1024];
  printf_vsn(ctx, sizeof(ctx), format, args);
  va_end(args);

  if (priority <= 4) {
    journal_file((priority <= 3) ? "ERR" : "WRN", "System Event", ctx);
  }
}

void journal_log(error_code code, ...) {
  msg_t msg = journal_get(code);
  char ctx[1024];
  va_list args;
  va_start(args, code);
  printf_vsn(ctx, sizeof(ctx), msg.details, args);
  va_end(args);

  const char *level = ((int)code < 0) ? "ERR" : "INF";
  journal_file(level, msg.summary, ctx);

  if ((int)code <= -30) {
    daemon_failsafe(-1);
  }
}

void journal_exit(int exit_code, const char *format, ...) {
  va_list args;
  va_start(args, format);

  char ctx[512];
  printf_vsn(ctx, sizeof(ctx), format, args);
  va_end(args);

  const char *ftl_msg = "   ${COLOR_RED}${XOUT} FATAL: ${COLOR_RESET}Daemon termination forced. [FATAL]\n";
  write(2, ftl_msg, strlen(ftl_msg));

  journal_file("FTL", "Daemon Termination", ctx);
  journal_write(LOG_ERR, "FATAL EXIT: %s", ctx);

  daemon_failsafe(-1);
  _exit(exit_code);
}

void journal_debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  char ctx[1024];
  printf_vsn(ctx, sizeof(ctx), format, args);
  va_end(args);

  journal_file("DBG", "Debug Event", ctx);
}

void journal_diag(int level, const char *format, ...) {
  (void)level;
  va_list args;
  va_start(args, format);
  char ctx[1024];
  printf_vsn(ctx, sizeof(ctx), format, args);
  va_end(args);

  journal_file("DBG", "Diagnostic Event", ctx);
}

/* end of `ERROR.C` */