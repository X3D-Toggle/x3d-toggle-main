/* Status reporting model for the X3D Toggle Project
 *
 * `status.c`
 *
 * Parses and passes relevant indicators for topology and modes to frontend
 */

#include "xui.h"
#include "ccd.h"
#include "modes.h"
#include "ipc.h"
#include "status.h"
#include "libc.h"

#ifndef CCD1_START
#define CCD1_START 8
#endif


void stats(CPUStats *st) {
  int fd = open("/proc/stat", O_RDONLY);
  if (fd < 0) return;
  char buf[256];
  ssize_t n = read(fd, buf, sizeof(buf)-1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';
  char *p = strchr(buf, ' ');
  if (!p) return;
  st->user = strtoull(p, &p, 10);
  st->nice = strtoull(p, &p, 10);
  st->system = strtoull(p, &p, 10);
  st->idle = strtoull(p, &p, 10);
  st->iowait = strtoull(p, &p, 10);
  st->irq = strtoull(p, &p, 10);
  st->softirq = strtoull(p, &p, 10);
}

double status_cpu(CPUStats *prev, CPUStats *curr) {
  unsigned long long p_idle = prev->idle + prev->iowait;
  unsigned long long c_idle = curr->idle + curr->iowait;
  unsigned long long p_non_idle = prev->user + prev->nice + prev->system + prev->irq + prev->softirq;
  unsigned long long c_non_idle = curr->user + curr->nice + curr->system + curr->irq + curr->softirq;
  unsigned long long p_total = p_idle + p_non_idle;
  unsigned long long c_total = c_idle + c_non_idle;
  unsigned long long diff_total = c_total - p_total;
  unsigned long long diff_idle = c_idle - p_idle;
  if (diff_total == 0) return 0.0;
  return (double)(diff_total - diff_idle) / (double)diff_total;
}

static void status_sysfs(const char *path, char *out, size_t max_len) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    scat(out, "N/A", max_len);
    return;
  }
  ssize_t n = read(fd, out, max_len - 1);
  if (n > 0) {
    out[n] = '\0';
    for (int i = 0; i < n; i++)
      if (out[i] == '\n' || out[i] == '\r')
        out[i] = '\0';
  } else {
    scat(out, "N/A", max_len);
  }
  close(fd);
}

static void status_upper(char *str) {
  for (int idx = 0; str[idx]; idx++) {
    str[idx] = toupper((unsigned char)str[idx]);
  }
}

static int daemon_active(void) {
  if (access(IPC_PATH, F_OK) != 0) {
    if (access("/var/run/x3d-toggle.pid", F_OK) != 0) {
      return 1;
    }
  }

  int res = socket_probe();
  if (res != 0) {
    if (errno == 13 /* EACCES */) return 3;
    return 2;
  }
  return 0;
}

int daemon_status(Status *st) {
  if (!st)
    return -1;
  memset(st, 0, sizeof(Status));

  int daemon_state = daemon_active();

  char c_mode_raw[BUFF_MODE] = "n/a";
  mode(c_mode_raw, sizeof(c_mode_raw));

  const char *ccd0_st = "ONLINE";
  const char *ccd1_st = "ONLINE";
  char path[BUFF_PATH];

  // CCD0 Recognition (Mimicking Discovery Logic)
  printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", 0);
  char b0[BUFF_STATE] = "1";
  int fd0 = open(path, O_RDONLY);
  if (fd0 >= 0) {
    if (read(fd0, b0, sizeof(b0) - 1) > 0) {
      if (b0[0] == '0')
        ccd0_st = "PARKED";
    }
    close(fd0);
  }

  // CCD1 Logic
  printf_sn(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", CCD1_START);
  char state_buf[BUFF_STATE] = "1";
  int fd = open(path, O_RDONLY);
  if (fd >= 0) {
    if (read(fd, state_buf, sizeof(state_buf) - 1) > 0) {
      if (state_buf[0] == '0') {
        ccd1_st = "PARKED";
      }
    }
    close(fd);
  }

  scat(st->c_mode, c_mode_raw, sizeof(st->c_mode));
  status_upper(st->c_mode);

  status_sysfs("/sys/devices/system/cpu/amd_pstate/status", st->d_buff, sizeof(st->d_buff));
  status_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", st->epp, sizeof(st->epp));
  status_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", st->gov, sizeof(st->gov));
  status_sysfs("/sys/devices/system/cpu/smt/control", st->smt, sizeof(st->smt));
  status_sysfs("/sys/firmware/acpi/platform_profile", st->plat, sizeof(st->plat));

  char bst_raw[BUFF_RAW];
  status_sysfs("/sys/devices/system/cpu/cpufreq/boost", bst_raw, sizeof(bst_raw));
  scat(st->st_buff, (strcmp(bst_raw, "1") == 0) ? "ENABLED" : "DISABLED", sizeof(st->st_buff));

  status_upper(st->d_buff);
  status_upper(st->epp);
  status_upper(st->gov);
  status_upper(st->smt);
  status_upper(st->plat);
  status_upper(st->st_buff);

  scat(st->daemon_state, "MANUAL", sizeof(st->daemon_state));
  scat(st->ebpf_status, "DYNAMIC POLLING", sizeof(st->ebpf_status));
  int live_override = -1;
  char live_info[BUFF_INFO] = "";

  if (daemon_state == 0 && socket_send("DAEMON_INFO", live_info, sizeof(live_info)) == 0) {
    char *st_val = strstr(live_info, "STATE=");
    char *ov_val = strstr(live_info, "OVERRIDE=");
    char *ba_val = strstr(live_info, "BPF_ACTIVE=");
    char *ri = strstr(live_info, "REFRESH_INTERVAL=");

    if (st_val) {
      scat(st->daemon_state, st_val + 6, sizeof(st->daemon_state));
      char *sc = strchr(st->daemon_state, ';');
      if (sc) *sc = '\0';
    }
    if (ov_val)
      live_override = atoi(ov_val + 9);
    if (ba_val)
      scat(st->ebpf_status,
              (atoi(ba_val + 11) ? "eBPF (Active)" : "Polling (Fallback)"),
              sizeof(st->ebpf_status));
    if (ri)
      st->refresh_interval = atof(ri + 17);
    else
      st->refresh_interval = 0.5;

    status_upper(st->daemon_state);
    if (strcmp(st->daemon_state, "DEFAULT") != 0) {
      scat(st->ebpf_status, "INACTIVE (MANUAL OVERRIDE)",
              sizeof(st->ebpf_status));
    }
  } else {
    int fd_conf = open(CONFIG_PATH, O_RDONLY);
    if (fd_conf >= 0) {
      char conf_buf[4096];
      ssize_t n_conf = read(fd_conf, conf_buf, sizeof(conf_buf) - 1);
      if (n_conf > 0) {
        conf_buf[n_conf] = '\0';
        char *line = conf_buf;
        char *nxt;
        while (line && *line) {
          nxt = strchr(line, '\n');
          if (nxt)
            *nxt = '\0';
          if (strncmp(line, "DAEMON_STATE=", 13) == 0) {
            scat(st->daemon_state, line + 13, sizeof(st->daemon_state));
            status_upper(st->daemon_state);
          }
          if (nxt)
            line = nxt + 1;
          else
            break;
        }
        if (strcmp(st->daemon_state, "DEFAULT") == 0) {
          scat(st->ebpf_status, "eBPF/POLLING (ACTIVE)", sizeof(st->ebpf_status));
        } else {
          scat(st->ebpf_status, "INACTIVE (SUSPENDED)", sizeof(st->ebpf_status));
        }
      }
      close(fd_conf);
    }
  }


  scat(st->st_buff_ccd0, ccd0_st, sizeof(st->st_buff_ccd0));
  scat(st->st_buff_ccd1, ccd1_st, sizeof(st->st_buff_ccd1));

  st->c_icon = QUERY;
  if (strstr(st->c_mode, "CACHE")) {
    st->c_icon = CACHED;
    scat(st->c_mode, "CACHE", sizeof(st->c_mode));
  } else if (strstr(st->c_mode, "FREQUENCY")) {
    st->c_icon = FREQU;
    scat(st->c_mode, "FREQUENCY", sizeof(st->c_mode));
  } else if (strstr(st->c_mode, "AUTO") || strstr(st->c_mode, "CPPC")) {
    st->c_icon = CHICKEN;
    scat(st->c_mode, "CPPC", sizeof(st->c_mode));
  } else {
    st->c_icon = QUERY;
    scat(st->c_mode, "UNKNOWN", sizeof(st->c_mode));
  }

  st->ccd_icon = HUT;
  scat(st->ccd_state, "OPTIMIZED", sizeof(st->ccd_state));
  if (strcmp(ccd1_st, "ONLINE") == 0) {
    if (live_override == 4) {
      st->ccd_icon = TOPSWAP;
      scat(st->ccd_state, "INVERTED", sizeof(st->ccd_state));
    } else {
      st->ccd_icon = DUALIZE;
      scat(st->ccd_state, "FULL THROUGHPUT", sizeof(st->ccd_state));
    }
  } else {
    st->ccd_icon = PINNED;
    scat(st->ccd_state, "CCD ISOLATED", sizeof(st->ccd_state));
  }

  if (daemon_state == 0) {
    scat(st->ipc_status, "SOCKET ONLINE", sizeof(st->ipc_status));
    if (strcmp(st->daemon_state, "DEFAULT") == 0) {
      st->d_icon = CHICKEN;
      scat(st->d_mode, "ACTIVE", sizeof(st->d_mode));
    } else if (strcmp(st->daemon_state, "HARD_RESET") == 0 || strcmp(st->daemon_state, "AUTO") == 0) {
      st->d_icon = STOPSIGN;
      scat(st->d_mode, "CPPC NATIVE", sizeof(st->d_mode));
    } else {
      st->d_icon = MANUAL;
      scat(st->d_mode, "SUSPENDED", sizeof(st->d_mode));
    }
  } else if (daemon_state == 2) {
    st->d_icon = XOUT;
    scat(st->ipc_status, "SOCKET OFFLINE", sizeof(st->ipc_status));
    scat(st->d_mode, "FAILED (NO IPC)", sizeof(st->d_mode));
    scat(st->ebpf_status, "INACTIVE", sizeof(st->ebpf_status));
  } else if (daemon_state == 3) {
    st->d_icon = PADLOCK;
    scat(st->ipc_status, "ACCESS DENIED", sizeof(st->ipc_status));
    scat(st->d_mode, "PERMISSION ERROR", sizeof(st->d_mode));
    scat(st->ebpf_status, "INACTIVE (RE-AUTH REQ)", sizeof(st->ebpf_status));
  } else {
    st->d_icon = STOPSIGN;
    scat(st->ipc_status, "SOCKET OFFLINE", sizeof(st->ipc_status));
    scat(st->d_mode, "STOPPED", sizeof(st->d_mode));
    scat(st->ebpf_status, "INACTIVE", sizeof(st->ebpf_status));
  }
  return 0;
}

/* end of STATUS.C */
