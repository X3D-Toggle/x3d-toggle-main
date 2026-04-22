/* Topology and Thread Management for the X3D Toggle Project
 *
 * `modes.c` - Backend Migration
 *
 * Strictly handles macro-level topology changes, hardware power-state
 * heuristics, and explicit backgrounding of the autonomous orchestration loop
 * via IPC.
 */

#include "modes.h"
#include "../build/config.h"
#include "../build/xui.h"
#include "cli.h"
#include "error.h" // IWYU pragma: keep
#include "libc.h"
#include "ipc.h"
#include "systemd.h"
#include "scheduler.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static char x3d_path_cache[256] = "";

static const char *find_path(void) {
  if (x3d_path_cache[0] != '\0')
    return x3d_path_cache;

  const char *targets[] = {
      "/sys/class/amd_x3d/vcache_mode",
      "/sys/bus/platform/devices/AMDI0101:00/amd_x3d_mode",
      "/sys/devices/platform/AMDI0101:00/amd_x3d_mode",
      "/sys/bus/pci/drivers/amd_x3d_vcache/vcache_mode",
      "/sys/bus/platform/drivers/amd_x3d_vcache/AMDI0101:00/amd_x3d_mode"};

  for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
    if (access(targets[i], R_OK) == 0) {
      scat(x3d_path_cache, targets[i], sizeof(x3d_path_cache));
      return x3d_path_cache;
    }
  }
  return NULL;
}

int mode(char *current_mode, size_t max_len) {
  const char *path = find_path();
  if (!path) {
    scat(current_mode, "unknown", max_len);
    return ERR_HW;
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
  if (!path)
    return ERR_HW;
  scat(buf, path, size);
  return ERR_SUCCESS;
}

int cli_set_mode(const char *target) {
  const char *path = find_path();
  if (!path)
    return ERR_HW;
  int fd = open(path, O_WRONLY);
  if (fd < 0)
    return ERR_HW;
  ssize_t n = write(fd, target, strlen(target));
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_IO;
}

int cli_set_dual(void) { return cli_set_mode("dual"); } // isnt this redundant
int cli_set_swap(void) { return cli_set_mode("swap"); } // isnt this redundant

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

/**
 * x3d_mode_apply - Orchestrates hardware and OS state transitions
 * @mode: The target partition (PART_DUAL, PART_CACHE, or PART_FREQ)
 *
 * Returns: 0 on success, -1 on failure
 */
int cli_mode_apply(int mode) {
    /* If running as non-root user (CLI standalone via udev permissions),
     * we cannot change cgroups or kernel scheduling. Skip OS-level transitions.
     */
    if (geteuid() != 0) {
        return 0;
    }

    /* 1. Hardware State Transition (CCD Affinity) 
     * Applies the partition to the current process (pid 0) 
     */
    int aff_res = affinity_partition(0, mode);
    if (aff_res != 0) {
        journal_error(ERR_AFFINITY, aff_res);
        return -1;
    }

    /* 2. OS Scheduler Optimization 
     * We trigger SCHED_GAMING (3ms slices + BORE Shift 14) only when 
     * the cache-focused CCD is prioritized. Otherwise, we restore Balanced.
     */
    sched_t target_sched = (mode == PART_CACHE) ? SCHED_GAMING : SCHED_BALANCED;
    
    if (scheduler_set(target_sched) != 0) {
        /* We log a warning but don't fail the hardware transition, 
         * as the system is still functional without the scheduler tweak.
         */
        journal_warn(ERR_HW);
    }

    return 0;
}

static int ccd_change(const char *mode) {
  int hw_res = ERR_SUCCESS;
  char display_mode[BUFF_DISPLAY];
  const char *status_text = "profile";

  if (strcmp(mode, "cache") == 0) {
    scat(display_mode, "LATENCY-OPTIMIZED", sizeof(display_mode));
  } else if (strcmp(mode, "frequency") == 0) {
    scat(display_mode, "THROUGHPUT-OPTIMIZED", sizeof(display_mode));
  } else {
    scat(display_mode, mode, sizeof(display_mode));
    for (int i = 0; display_mode[i]; i++) {
      if (display_mode[i] >= 'a' && display_mode[i] <= 'z')
        display_mode[i] -= 32;
    }
  }
  display_mode[sizeof(display_mode) - 1] = '\0';

  int ipc_res = ERR_SUCCESS;
  if (strcmp(mode, "cache") == 0 || strcmp(mode, "frequency") == 0 ||
      strcmp(mode, "dual") == 0 || strcmp(mode, "swap") == 0) {
    socket_send("SET_DAEMON manual", NULL, 0);
    char cmd[64];
    printf_sn(cmd, sizeof(cmd), "SET_MODE %s", mode);
    ipc_res = socket_send(cmd, NULL, 0);
  }

  if (strcmp(mode, "dual") == 0) {
    hw_res = cli_set_dual();
    cli_mode_apply(PART_DUAL);
  } else if (strcmp(mode, "swap") == 0) {
    hw_res = cli_set_swap();
    cli_mode_apply(PART_FREQ);
  } else {
    hw_res = cli_set_mode(mode);
    if (strcmp(mode, "cache") == 0) {
      cli_mode_apply(PART_CACHE);
    } else if (strcmp(mode, "frequency") == 0) {
     cli_mode_apply(PART_FREQ);
    }
  }

  if (hw_res != ERR_SUCCESS) {
    journal_error(ERR_HW, hw_res);
    return ERR_HW;
  }

  char path[256] = "unknown";
  mode_path(path, sizeof(path));
  
  if (ipc_res != ERR_SUCCESS) {
    printf_step("${ALRIGHT} ${COLOR_CYAN}%s${COLOR_RESET} %s applied via ${COLOR_CYAN}%s${COLOR_RESET} - ${COLOR_YELLOW}IPC socket offline",
                display_mode, status_text, path);
  } else {
    printf_step("${ALRIGHT} ${COLOR_CYAN}%s${COLOR_RESET} %s applied via ${COLOR_CYAN}%s",
                display_mode, status_text, path);
  }

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
  printf_step("${RELOAD} Initiating HARD RESET: Restoring CPPC defaults and purging session...");

  system("X3D_EXEC=1 sh /usr/lib/x3d-toggle/scripts/tools/reset.sh");

  unit_disable();
  unit_stop();

  printf_step("${ALRIGHT} ${COLOR_CYAN}CPPC NATIVE ${COLOR_RESET}control restored. Service disabled.");
  return 0;
}

int cli_mode_reset(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${RELOAD} Initiating Hardware Re-probe: Restoring native heuristics...");

  int res = ccd_change("reset");

  if (unit_stop() == 0) {
    printf_step("${PAUSE} Autonomous service ${COLOR_CYAN}STOPPED${COLOR_RESET}.");
  }

  return res;
}

int cli_mode_default(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${RELOAD} SOFT RESET: Restoring default daemon rules and throughput baseline...");
  config_update("DAEMON_STATE", "default");
  config_update("POLLING_INTERVAL", TOSTRING(CONFIG_POLLING_INTERVAL));
  config_update("LOAD_THRESHOLD", TOSTRING(CONFIG_LOAD_THRESHOLD));
  config_update("DETECTION_LEVEL", TOSTRING(CONFIG_DETECTION_LEVEL));
  config_update("EBPF_ENABLE", TOSTRING(CONFIG_EBPF_ENABLE));

  int r1 = socket_send("CONFIG_SYNC", NULL, 0);
  int r2 = socket_send("SET_DAEMON default", NULL, 0);
  int r3 = socket_send("SET_MODE frequency", NULL, 0);

  if (r1 == 0 && r2 == 0 && r3 == 0) {
    printf_step("${ALRIGHT} ${COLOR_CYAN}DEFAULT CONFIG${COLOR_RESET} restored. Orchestration active.");
  } else {
    printf_step("${WARN} ${COLOR_YELLOW}CONFIG UPDATED${COLOR_RESET} but Daemon is offline. Restart to apply.");
  }
  return 0;
}

/* end of MODES.C */
