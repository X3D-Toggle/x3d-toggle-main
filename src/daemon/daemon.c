/* Automation backend for the X3D Toggle Project
 *
 * `daemon.c`
 *
 * Orchestrates the logistics of automated polling and eBPF functionality
 * while ensuring user-defined settings are persistent. Mimics the behavior
 * of the native Windows driver by deterministically switching CCD priority.
 * Utilizes the shared IPC module for passwordless CLI configuration.
 */

#include "daemon.h"
#include "bpf-user.h"
#include "config.h"
#include "cppc.h"
#include "error.h"
#include "games.h"
#include "ipc.h"
#include "polling.h"
#include "status.h"
#include "systemd.h"
#include "xui.h"

extern char **environ;

extern volatile int socket_ready;

DaemonConfig cfg;
gamelist gl;
bool bpf_active = false;

#ifndef CLI_BUILD
int daemon(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  CPUStats p_stat = {0}, c_stat = {0};
  char current[32], target[32];
  int server_fd;

  init_signals();
  openlog("x3d-daemon", LOG_PID, LOG_DAEMON);

  config_load(&cfg);
  journal_info(DAEMON_START,
               "X3D Toggle Daemon starting (Session Log Initialized)");
  games_load(&gl);
  if (affinity_init() != ERR_SUCCESS) {
    journal_error(ERR_CGROUP, "Failed to initialize ephemeral partitions.");
  }

  if (cfg.ebpf_enable) {
    bpf_active = bpf_init();
  }

  cli_set_mode("frequency");

  if (strlen(cfg.daemon_state) == 0) {
    scat(cfg.daemon_state, "default", sizeof(cfg.daemon_state));
  }
  
  /* Enforce startup baseline: Default to Frequency (Throughput) */
  if (strcmp(cfg.daemon_state, "default") == 0) {
    cli_set_mode("frequency");
  }

  server_fd = socket_setup();
  if (server_fd < 0) {
    journal_exit(1, "Failed to initialize IPC server");
  }

  notify_ready();
  stats(&p_stat);

  fd_set readfds;
  struct timeval tv;

  while (active_keep) {
    FD_ZERO(&readfds);
    if (server_fd >= 0)
      FD_SET(server_fd, &readfds);

    tv.tv_sec = (time_t)cfg.polling_interval;
    tv.tv_usec = 0;

    int sel = select(server_fd + 1, &readfds, NULL, NULL, &tv);

    if (sel > 0 && FD_ISSET(server_fd, &readfds)) {
      socket_handle(server_fd);
    }

    if (!active_keep)
      break;

    polling_run(&p_stat, &c_stat, current, target);

    if (reload_flag) {
      reload_flag = 0;
      config_load(&cfg);
      games_load(&gl);
    }
  }

  log_shutdown();

  if (server_fd >= 0)
    close(server_fd);
  if (bpf_active)
    bpf_cleanup();

  cppc_restore();
  unlink(IPC_PATH);
  closelog();

  return 0;
}
#endif

int cli_framework(int argc, char *argv[]) {
  if (argc < 3) {
    journal_error(ERR_SYNTAX, "Usage: x3d framework [ARGS...]");
    return ERR_SYNTAX;
  }

  printf_step("${GEAR} Initiating Framework Orchestration...");

  pid_t pid = fork();
  if (pid == 0) {
    char *framework_path =
        (char *)"/usr/lib/x3d-toggle/scripts/framework/framework.sh";

    char *args[32];
    int idx = 0;
    args[idx++] = (char *)"/usr/bin/sh";
    args[idx++] = framework_path;
    for (int i = 2; i < argc && idx < 31; i++) {
      args[idx++] = argv[i];
    }
    args[idx] = NULL;

    char *env[] = {(char *)"X3D_EXEC=1", (char *)"X3D_FRAMEWORK=1",
                   (char *)"X3D_TOGGLE=.", NULL};

    execve(args[0], args, env);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS
                                                           : ERR_IO;
  }
  return ERR_IO;
}

int cli_daemon_enable(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${ROCKET} Initializing X3D Toggle Daemon (Current Session)...");

  config_update("DAEMON_STATE", "default");

  int res = unit_start();
  if (res == 0) {
    int retries = 5;
    int ipc_ok = 0;
    while (retries-- > 0) {
      if (socket_send("SET_DAEMON default", NULL, 0) == 0) {
        ipc_ok = 1;
        break;
      }
      struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000L}; // 100ms
      nanosleep(&ts, NULL);
    }

    if (!ipc_ok) {
      journal_error(ERR_IPC, -1);
    }

    printf_step("${ALRIGHT} Soft Wake: Daemon started and automation ENABLED "
                "for current session.");
    return 0;
  }

  journal_error(ERR_DAEMON,
                "Failed to start systemd service via DBus. (Code: %d)", res);
  return 1;
}

int cli_daemon_start(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int res = unit_enable();
  if (res != 0) {
    journal_error(ERR_DAEMON,
                  "Failed to register autostart via DBus. (Code: %d)", res);
  }

  return cli_daemon_enable(argc, argv);
}

int cli_daemon_wake(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int r1 = socket_send("CONFIG_SYNC", NULL, 0);
  int r2 = socket_send("SET_DAEMON default", NULL, 0);
  int r3 = socket_send("SET_MODE frequency", NULL, 0);

  if (r1 == 0 && r2 == 0 && r3 == 0) {
    printf_step("${CHICKEN} Poke: Daemon orchestration re-enabled via IPC.");
    return 0;
  }

  journal_error(
      ERR_IPC,
      "Failed to poke daemon (Code: %d). Is the service actively running?",
      ipc_res);
  return 1;
}

int cli_daemon_sleep(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int ipc_res = socket_send("SET_DAEMON manual", NULL, 0);

  if (ipc_res == 0) {
    printf_step("${SLEEPY} Firm Sleep: Daemon logic loop suspended.");
    printf_step("Manual hardware overrides will now persist without daemon "
                "interference.");
    return 0;
  }

  journal_error(
      ERR_IPC,
      "Failed to suspend daemon (Code: %d). Is the service actively running?",
      ipc_res);
  return 1;
}

int cli_daemon_stop(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${STOPSIGN} Terminating Daemon...");

  if (unit_stop() == 0) {
    printf_step("${ALRIGHT} Daemon gracefully STOPPED.");
    return 0;
  }

  journal_error(ERR_DAEMON, "Failed to stop systemd service via DBus.");
  return 1;
}

int cli_daemon_disable(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${WIPE} Unregistering Daemon autostart rules...");

  config_update("DAEMON_STATE", "manual");

  int res = unit_disable();
  if (res == 0) {
    printf_step(
        "${ALRIGHT} Success: x3d-toggle.service is now disabled and stopped.");
    return 0;
  }

  journal_error(res, "Could not disable x3d-toggle.service via DBus.");
  return 1;
}
