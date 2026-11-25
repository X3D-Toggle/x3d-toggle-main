/* Topology and Thread Management for the X3D Toggle Project
 *
 * `modes.c` - Backend Migration
 *
 * Strictly handles macro-level topology changes, hardware power-state
 * heuristics, and explicit backgrounding of the autonomous orchestration loop
 * via IPC.
 */

#include "modes.h"
#include "ccd.h"
#include "../build/config.h"
#include "../build/xui.h"
#include "cli.h"
#include "error.h" // IWYU pragma: keep
#include "ipc.h"
#include "libc.h"
#include "scheduler.h"
#include "systemd.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static char x3d_path_cache[256] = "";

static const char *find_path(void) {
  if (x3d_path_cache[0] != '\0')
    return x3d_path_cache;

  const char *targets[] = {
      "/sys/bus/platform/drivers/amd_x3d_vcache/AMDI0101:00/amd_x3d_mode",
      "/sys/devices/platform/AMDI0101:00/amd_x3d_mode",
      "/sys/bus/platform/devices/AMDI0101:00/amd_x3d_mode",
      "/sys/class/amd_x3d/vcache_mode",
      "/sys/bus/pci/drivers/amd_x3d_vcache/vcache_mode"};

  for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
    if (access(targets[i], R_OK) == 0) {
      printf_sn(x3d_path_cache, sizeof(x3d_path_cache), "%s", targets[i]);
      return x3d_path_cache;
    }
  }

  /* Dynamic ACPI scan for future AMDI* 3D V-Cache HIDs (Linux 7.x+ / Zen 6+) */
  int dir_fd = open("/sys/devices/platform", O_RDONLY);
  if (dir_fd >= 0) {
    char dbuf[2048];
    int nread;
    while ((nread = getdents64((unsigned int)dir_fd,
                               (struct linux_dirent64 *)dbuf,
                               sizeof(dbuf))) > 0) {
      for (int bpos = 0; bpos < nread;) {
        struct linux_dirent64 *d = (struct linux_dirent64 *)(dbuf + bpos);
        if (strncmp(d->d_name, "AMDI", 4) == 0) {
          char candidate[256];
          printf_sn(candidate, sizeof(candidate),
                    "/sys/devices/platform/%s/amd_x3d_mode", d->d_name);
          if (access(candidate, R_OK) == 0) {
            printf_sn(x3d_path_cache, sizeof(x3d_path_cache), "%s", candidate);
            close(dir_fd);
            return x3d_path_cache;
          }
        }
        bpos += d->d_reclen;
      }
    }
    close(dir_fd);
  }

  return NULL;
}

int mode(char *current_mode, size_t max_len) {
  const char *path = find_path();
  if (!path) {
#if defined(SYMMETRIC_X3D) && SYMMETRIC_X3D
    printf_sn(current_mode, max_len, "%s",
              (CCD_COUNT > 1) ? "dual-vcache" : "vcache");
    return ERR_SUCCESS;
#else
    printf_sn(current_mode, max_len, "unknown");
    return ERR_HW;
#endif
  }
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return ERR_HW;
  ssize_t n = read(fd, current_mode, max_len - 1);
  close(fd);
  if (n <= 0)
    return ERR_IO;
  current_mode[n] = '\0';
  char *p = strchr(current_mode, '\n');
  if (p)
    *p = '\0';
  return ERR_SUCCESS;
}

int mode_path(char *buf, size_t size) {
  const char *path = find_path();
  if (!path) {
#if defined(SYMMETRIC_X3D) && SYMMETRIC_X3D
    printf_sn(buf, size, "symmetric-3d-vcache (cgroup/CPPC)");
    return ERR_SUCCESS;
#else
    return ERR_HW;
#endif
  }
  printf_sn(buf, size, "%s", path);
  return ERR_SUCCESS;
}

int cli_set_mode(const char *target) {
  const char *path = find_path();
  if (!path) {
#if defined(SYMMETRIC_X3D) && SYMMETRIC_X3D
    (void)target;
    return ERR_SUCCESS;
#else
    return ERR_HW;
#endif
  }
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_HW;
  ssize_t n = write(fd, target, strlen(target));
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_IO;
}

int cli_set_dual(void) {
  for (int i = 1; i < TOTAL_CORES; i++) {
    cli_set_core(i, 1);
  }
  if (find_path() != NULL) {
    return cli_set_mode("frequency");
  }
  return ERR_SUCCESS;
}

int cli_set_swap(void) {
  char cur[32] = {0};
  if (mode(cur, sizeof(cur)) == ERR_SUCCESS && strcmp(cur, "cache") == 0) {
    return cli_set_mode("frequency");
  }
  return cli_set_mode("cache");
}

int cli_set_core(int core_id, int online) {
  char path[128];
  printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online",
            core_id);
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_HW;
  const char *val = online ? "1" : "0";
  ssize_t n = write(fd, val, 1);
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_IO;
}

int cli_mode_apply(int target_mode) {
  if (geteuid() != 0) {
    return 0;
  }

  int aff_res = affinity_partition(0, target_mode);
  if (aff_res != 0) {
    journal_error(ERR_AFFINITY, aff_res);
    return -1;
  }

  sched_t target_sched = (target_mode == PART_CACHE) ? SCHED_GAMING : SCHED_BALANCED;

  if (scheduler_set(target_sched) != 0) {
    journal_warn(ERR_HW);
  }

  return 0;
}

static int ccd_change(const char *target_mode) {
  char display_mode[BUFF_DISPLAY];
  const char *status_text = "profile";

  if (strcmp(target_mode, "cache") == 0) {
    printf_sn(display_mode, sizeof(display_mode), "LATENCY-OPTIMIZED");
  } else if (strcmp(target_mode, "frequency") == 0) {
    printf_sn(display_mode, sizeof(display_mode), "THROUGHPUT-OPTIMIZED");
  } else {
    printf_sn(display_mode, sizeof(display_mode), "%s", target_mode);
    for (int i = 0; display_mode[i]; i++) {
      if (display_mode[i] >= 'a' && display_mode[i] <= 'z')
        display_mode[i] -= 32;
    }
  }
  display_mode[sizeof(display_mode) - 1] = '\0';

  int ipc_res = ERR_IPC;
  if (strcmp(target_mode, "cache") == 0 || strcmp(target_mode, "frequency") == 0 ||
      strcmp(target_mode, "dual") == 0 || strcmp(target_mode, "swap") == 0) {
    socket_send("SET_DAEMON manual", NULL, 0);
    char cmd[64];
    printf_sn(cmd, sizeof(cmd), "SET_MODE %s", target_mode);
    ipc_res = socket_send(cmd, NULL, 0);
  }

  if (ipc_res == ERR_SUCCESS) {
    char path[256] = "unknown";
    mode_path(path, sizeof(path));
    printf_step("${ALRIGHT} ${COLOR_CYAN}%s${COLOR_RESET} %s applied via "
                "${COLOR_CYAN}%s",
                display_mode, status_text, path);
    return ERR_SUCCESS;
  }

  int hw_res = ERR_SUCCESS;
  if (strcmp(target_mode, "dual") == 0) {
    hw_res = cli_set_dual();
  } else if (strcmp(target_mode, "swap") == 0) {
    hw_res = cli_set_swap();
  } else {
    hw_res = cli_set_mode(target_mode);
  }

  if (hw_res != ERR_SUCCESS) {
    journal_error(ERR_HW, hw_res);
    return ERR_HW;
  }

  char path[256] = "unknown";
  mode_path(path, sizeof(path));
  printf_step(
      "${ALRIGHT} ${COLOR_CYAN}%s${COLOR_RESET} %s applied via "
      "${COLOR_CYAN}%s${COLOR_RESET} - ${COLOR_YELLOW}IPC socket offline",
      display_mode, status_text, path);

  return ERR_SUCCESS;
}

int cli_mode_cache(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return ccd_change("cache");
}

int cli_mode_frequency(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return ccd_change("frequency");
}

int cli_mode_dual(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return ccd_change("dual");
}

int cli_mode_swap(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return ccd_change("swap");
}

int cli_mode_auto(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${RELOAD} Initiating HARD RESET: Restoring CPPC defaults and "
              "purging session...");

  pid_t pid = fork();
  if (pid == 0) {
    char *args[] = {(char *)"/bin/sh",
                    (char *)"/usr/lib/x3d-toggle/scripts/tools/reset.sh",
                    NULL};
    char *envp[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, envp);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    int status;
    (void)waitpid(pid, &status, 0);
  }

  if (socket_send("DAEMON_DISABLE", NULL, 0) != ERR_SUCCESS) {
    unit_disable();
    unit_stop();
  }

  printf_step("${ALRIGHT} ${COLOR_CYAN}CPPC NATIVE ${COLOR_RESET}control "
              "restored. Service disabled.");
  return 0;
}

int cli_mode_reset(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step(
      "${RELOAD} Initiating Hardware Re-probe: Restoring native heuristics...");

  if (socket_send("SET_MODE reset", NULL, 0) == ERR_SUCCESS) {
    printf_step(
        "${ALRIGHT} Hardware re-probe complete via daemon IPC.");
    return ERR_SUCCESS;
  }

  int res = ccd_change("reset");
  return res;
}

int cli_mode_default(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (getenv("X3D_SETUP")) {
    printf_step("2,${SPARKLE} Initializing with default config daemon rules");
  } else {
    printf_step("2,${RELOAD} SOFT RESET: Restoring default daemon rules");
  }
  config_update("DAEMON_STATE", "default");
  config_update("POLLING_INTERVAL", TOSTRING(CONFIG_POLLING_INTERVAL));
  config_update("LOAD_THRESHOLD", TOSTRING(CONFIG_LOAD_THRESHOLD));
  config_update("DETECTION_LEVEL", TOSTRING(CONFIG_DETECTION_LEVEL));
  config_update("EBPF_ENABLE", TOSTRING(CONFIG_EBPF_ENABLE));

  int r1 = socket_send("CONFIG_SYNC", NULL, 0);
  int r2 = socket_send("SET_DAEMON default", NULL, 0);
  int r3 = socket_send("SET_MODE frequency", NULL, 0);

  if (r1 == 0 && r2 == 0 && r3 == 0) {
    if (getenv("X3D_SETUP")) {
      printf_step("2,${ALRIGHT} DEFAULT CONFIG initialized. Daemon active.");
    } else {
      printf_step("${ALRIGHT} DEFAULT CONFIG restored. Daemon active.");
    }
  } else {
    printf_step("${WARN} CONFIG UPDATED but daemon is offline. Restart to apply.");
  }
  return 0;
}

/* end of MODES.C */
