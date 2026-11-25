/* Scaling heuristics & P-State & EPP Management for the X3D Toggle Project
 * `cppc.c` - Backend Migration
 * Advanced Featureset: Handles EPP values, amd_pstate operational modes, preferred core toggles, and frequency governors via IPC.
 */

#include "../build/xui.h"
#include "cppc.h"
#include "ipc.h"
#include "status.h" // IWYU pragma: keep
#include "libc.h"
#include "../build/ccd.h"

#define _POSIX_C_SOURCE 202405L

extern char **environ;

int cppc_boost(int enable) {
  const char *val = enable ? "1" : "0";
  int fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
  if (fd >= 0) {
    ssize_t n = write(fd, val, 1);
    close(fd);
    return (n > 0) ? ERR_SUCCESS : ERR_IO;
  }

  /* Fallback: per-CPU amd-pstate cpufreq boost (Linux 6.11 - 7.x+) */
  int applied = 0;
  for (int i = 0; i < TOTAL_CORES; i++) {
    char path[256];
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/boost", i);
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
      if (write(fd, val, 1) > 0) applied++;
      close(fd);
    }
  }
  return (applied > 0) ? ERR_SUCCESS : ERR_HW;
}

int cppc_tdp(int watts) {
  journal_debug("Core: Requesting TDP limit adjustment to %d Watts", watts);
  int pid = fork();
  if (pid < 0) return ERR_IO;
  if (pid == 0) {
    char wbuf[16];
    printf_sn(wbuf, sizeof(wbuf), "%d", watts);
    char *args[] = {(char *)"/usr/bin/x3d-toggle", (char *)"framework", (char *)"--set-tdp", wbuf, NULL};
    execve(args[0], args, environ);
    _exit(1);
  }

  int status;
  waitpid(pid, &status, 0);
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
}

int cppc_perf(int val) {
  char val_str[16];
  printf_sn(val_str, sizeof(val_str), "%d", val);
  int ret = ERR_SUCCESS;
  int applied = 0;

  for (int i = 0; i < TOTAL_CORES; i++) {
    char path[256];
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/acpi_cppc/desired_perf", i);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
      if (write(fd, val_str, strlen(val_str)) < 0) ret = ERR_IO;
      else applied++;
      close(fd);
    }
  }
  if (applied == 0) {
    journal_warn(ERR_UNSUPPORTED, "acpi_cppc/desired_perf (unavailable when amd_pstate=active)");
    return ERR_UNSUPPORTED;
  }
  return ret;
}

int cppc_epp(const char *val) {
  int ret = ERR_SUCCESS;
  for (int i = 0; i < TOTAL_CORES; i++) {
    char path[256];
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/energy_performance_preference", i);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
      if (write(fd, val, strlen(val)) < 0) ret = ERR_IO;
      close(fd);
    }
  }
  return ret;
}

int cppc_ccd(int ccd_idx, const char *val) {
  const char *mask = (ccd_idx == 0) ? CCD0_CORE_MASK : CCD1_CORE_MASK;
  int ret = ERR_SUCCESS;
  for (int i = 0; i < TOTAL_CORES; i++) {
    if (!cpu_in_cpulist(i, mask))
      continue;
    char path[256];
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/energy_performance_preference", i);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
      if (write(fd, val, strlen(val)) < 0) ret = ERR_IO;
      close(fd);
    }
  }
  return ret;
}

int cppc_pstate(const char *val) {
  int fd = open("/sys/devices/system/cpu/amd_pstate/status", O_WRONLY);
  if (fd < 0) return ERR_HW;
  ssize_t n = write(fd, val, strlen(val));
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_IO;
}

int cppc_governor(const char *val) {
  int ret = ERR_SUCCESS;
  for (int i = 0; i < TOTAL_CORES; i++) {
    char path[256];
    printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
      if (write(fd, val, strlen(val)) < 0) ret = ERR_IO;
      close(fd);
    }
  }
  return ret;
}

int cppc_prefcore(int enable) {
  int fd = open("/sys/devices/system/cpu/amd_pstate/prefcore", O_WRONLY);
  if (fd < 0) {
    if (access("/sys/devices/system/cpu/amd_pstate/prefcore", R_OK) == 0) {
      /* Read-only sysfs node on Linux 6.13 - 7.x+ (controlled via amd_pstate.prefcore= cmdline) */
      journal_warn(ERR_UNSUPPORTED,
                   "amd_pstate/prefcore runtime write (configure via kernel parameter amd_pstate.prefcore=)");
      return ERR_UNSUPPORTED;
    }
    return ERR_HW;
  }
  const char *val = enable ? "enabled" : "disabled";
  ssize_t n = write(fd, val, strlen(val));
  if (n <= 0) {
    val = enable ? "1" : "0";
    n = write(fd, val, 1);
  }
  close(fd);
  return (n > 0) ? ERR_SUCCESS : ERR_IO;
}

int cppc_restore(void) {
  pid_t pid = fork();
  if (pid < 0) return ERR_IO;
  if (pid == 0) {
    char *args[] = {(char *)"/bin/sh",
                    (char *)"/usr/lib/x3d-toggle/scripts/tools/reset.sh",
                    NULL};
    char *envp[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, envp);
    _exit(EXIT_FAILURE);
  }

  int status;
  waitpid(pid, &status, 0);
  return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
}

static int cppc_cmd(const char *cmd_string, error_code success_code, const char *arg) {
    int res = socket_send(cmd_string, NULL, 0);
    if (res == ERR_SUCCESS) {
        if (arg) journal_info(success_code, arg);
        else journal_info(success_code);
        return ERR_SUCCESS;
    }
    journal_error(ERR_IPC, res);
    return ERR_IPC;
}

int cli_cppc_perf(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return cppc_cmd("SET_EPP performance", SET_EPP, "Maximum Performance (0x0)");
}

int cli_cppc_bal(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return cppc_cmd("SET_EPP balance_performance", SET_EPP, "Balanced Performance");
}

int cli_cppc_eco(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return cppc_cmd("SET_EPP balance_power", SET_EPP, "Balanced Power");
}

int cli_cppc_power(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return cppc_cmd("SET_EPP power", SET_EPP, "Minimum Power (0xff)");
}

int cli_cppc_epp(int argc, char *argv[]) {
    if (argc < 4) {
        journal_error(ERR_USAGE);
        return ERR_SYNTAX;
    }
    
    char payload[128];
    char arg_buf[128];
    printf_sn(payload, sizeof(payload), "SET_EPP_CCD %s %s", argv[2], argv[3]);
    printf_sn(arg_buf, sizeof(arg_buf), "%s -> %s", argv[2], argv[3]);
    
    return cppc_cmd(payload, SET_EPP, arg_buf);
}

int cli_cppc_raw(int argc, char *argv[]) {
    if (argc < 4) {
        journal_error(ERR_USAGE);
        return ERR_SYNTAX;
    }

    char payload[128];
    char arg_buf[128];
    int raw_value = atoi(argv[2]);
    printf_sn(payload, sizeof(payload), "SET_RAW %s %s", argv[3], argv[2]);
    printf_sn(arg_buf, sizeof(arg_buf), "0x%x applied to %s", (unsigned int)raw_value, argv[3]);
    
    return cppc_cmd(payload, SET_EPP, arg_buf);
}

int cli_cppc_mode(int argc, char *argv[]) {
    if (argc < 3) {
        journal_error(ERR_USAGE);
        return ERR_SYNTAX;
    }
    
    char payload[128];
    printf_sn(payload, sizeof(payload), "SET_PSTATE %s", argv[2]);
    
    return cppc_cmd(payload, SET_PSTATE, argv[2]);
}

int cli_cppc_target(int argc, char *argv[]) {
    if (argc < 3) {
        journal_error(ERR_SYNTAX, "Usage: x3d-toggle cppc-target [0-255]");
        return ERR_SYNTAX;
    }

    char payload[128];
    printf_sn(payload, sizeof(payload), "SET_CPPC %s", argv[2]);
    
    return cppc_cmd(payload, SET_CPPC, argv[2]);
}

int cli_cppc_governor(int argc, char *argv[]) {
    if (argc < 3) {
        journal_error(ERR_USAGE);
        return ERR_SYNTAX;
    }

    char payload[128];
    printf_sn(payload, sizeof(payload), "SET_GOVERNOR %s", argv[2]);
    
    return cppc_cmd(payload, TOGGLE_GOV, argv[2]);
}

int cli_cppc_prefcore(int argc, char *argv[]) {
    if (argc < 3) {
        journal_error(ERR_SYNTAX, "Usage: x3d-toggle prefcore [on|off]");
        return ERR_SYNTAX;
    }

    char payload[128];
    printf_sn(payload, sizeof(payload), "SET_PREFCORE %s", argv[2]);
    
    return cppc_cmd(payload, TOGGLE_PREFCORE, argv[2]);
}

/* end of CPPC.C */
