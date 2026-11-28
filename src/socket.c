/* Socket Server for the X3D Toggle Project
 * `socket.c`
 * Handles Daemon-side UNIX Domain Socket creation and listens for CLI commands.
 * Employs a non-blocking select() loop for graceful termination.
 */

#include "../build/ccd.h"
#include "../build/config.h"
#include "../build/xui.h"
#include "aod.h"
#include "cppc.h"
#include "daemon/bpf/bpf-user.h"
#include "error.h"
#include "games.h"
#include "ipc.h"
#include "irq.h"
#include "libc.h"
#include "modes.h"

#define MAX_IPC_BUFFER_SIZE 512
#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 5
#endif

extern volatile sig_atomic_t active_keep;
extern volatile int active_override;
extern DaemonConfig cfg;
extern bool bpf_active;
extern volatile int wrapper_pid;
extern volatile int poll_now;

static AodState g_aod_cache;
static int g_aod_probed = 0;

static void sock_read_sysfs(const char *path, char *out, size_t max_len) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf_sn(out, max_len, "N/A");
    return;
  }
  ssize_t n = read(fd, out, max_len - 1);
  close(fd);
  if (n > 0) {
    out[n] = '\0';
    for (ssize_t i = 0; i < n; i++) {
      if (out[i] == '\n' || out[i] == '\r') {
        out[i] = '\0';
        break;
      }
    }
  } else {
    printf_sn(out, max_len, "N/A");
  }
}

static void sock_to_upper(char *s) {
  for (size_t i = 0; s[i]; i++)
    s[i] = (char)toupper((unsigned char)s[i]);
}

static void sock_ensure_aod(void) {
  if (!g_aod_probed) {
    aod_probe_state(&g_aod_cache);
    g_aod_probed = 1;
  }
}

static void sock_format_status(char *out, size_t out_sz) {
  sock_ensure_aod();

  char d_state[32], d_mode[32], ebpf[32], sched[32];
  printf_sn(d_state, sizeof(d_state), "%s", cfg.daemon_state);
  sock_to_upper(d_state);

  printf_sn(ebpf, sizeof(ebpf), "%s",
            bpf_active ? "HEALTHY" : "POLLING (FALLBACK)");
  printf_sn(sched, sizeof(sched), "INACTIVE (MANUAL OVERRIDE)");

  if (strcmp(d_state, "DEFAULT") == 0) {
    printf_sn(d_mode, sizeof(d_mode), "ACTIVE");
    printf_sn(sched, sizeof(sched), "ACTIVE");
  } else if (strcmp(d_state, "HARD_RESET") == 0 ||
             strcmp(d_state, "AUTO") == 0) {
    printf_sn(d_mode, sizeof(d_mode), "CPPC NATIVE");
  } else {
    printf_sn(d_mode, sizeof(d_mode), "SUSPENDED");
    printf_sn(ebpf, sizeof(ebpf), "INACTIVE (MANUAL OVERRIDE)");
  }

  char vcache[32] = "N/A";
  mode(vcache, sizeof(vcache));
  sock_to_upper(vcache);

  char ccd0[16] = "ONLINE", ccd1[16] = "ONLINE";
  char b0[8] = "1";
  int fd = open("/sys/devices/system/cpu/cpu0/online", O_RDONLY);
  if (fd >= 0) {
    if (read(fd, b0, sizeof(b0) - 1) > 0 && b0[0] == '0')
      printf_sn(ccd0, sizeof(ccd0), "PARKED");
    close(fd);
  }
#ifndef CCD1_START
#define CCD1_START 8
#endif
#ifndef CCD_COUNT
#define CCD_COUNT 2
#endif
  if (CCD_COUNT >= 2 && CCD1_START > 0) {
    char p[128];
    printf_sn(p, sizeof(p), "/sys/devices/system/cpu/cpu%d/online", CCD1_START);
    b0[0] = '1';
    fd = open(p, O_RDONLY);
    if (fd >= 0) {
      if (read(fd, b0, sizeof(b0) - 1) > 0 && b0[0] == '0')
        printf_sn(ccd1, sizeof(ccd1), "PARKED");
      close(fd);
    }
  } else {
    printf_sn(ccd1, sizeof(ccd1), "SINGLE-CCD");
  }

  char ccd_state[32];
  int affinity_active = (cfg.affinity_mask[0] != '\0' &&
                         strncmp(cfg.affinity_mask, "none", 4) != 0);
  if (affinity_active)
    printf_sn(ccd_state, sizeof(ccd_state), "AFFINITY PINNED");
  else if (strcmp(ccd1, "ONLINE") == 0)
    printf_sn(ccd_state, sizeof(ccd_state), "%s",
              (active_override == 4) ? "INVERTED" : "FULL THROUGHPUT");
  else
    printf_sn(ccd_state, sizeof(ccd_state), "CCD ISOLATED");

  char drv[32], epp[32], dynepp[32], gov[32], smt[32], plat[32], bst_raw[16];
  sock_read_sysfs("/sys/devices/system/cpu/amd_pstate/status", drv,
                  sizeof(drv));
  sock_read_sysfs(
      "/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference", epp,
      sizeof(epp));
  sock_read_sysfs("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", gov,
                  sizeof(gov));
  sock_read_sysfs("/sys/devices/system/cpu/smt/control", smt, sizeof(smt));
  sock_read_sysfs("/sys/firmware/acpi/platform_profile", plat, sizeof(plat));
  if (strcmp(plat, "N/A") == 0)
    printf_sn(plat, sizeof(plat), "DESKTOP");

  if (access("/sys/devices/system/cpu/amd_pstate/dynamic_epp", R_OK) == 0) {
    sock_read_sysfs("/sys/devices/system/cpu/amd_pstate/dynamic_epp", dynepp,
                    sizeof(dynepp));
  } else {
    printf_sn(dynepp, sizeof(dynepp), "UNSUPPORTED");
  }

  sock_read_sysfs("/sys/devices/system/cpu/cpufreq/boost", bst_raw,
                  sizeof(bst_raw));
  const char *boost = (strcmp(bst_raw, "1") == 0) ? "ENABLED" : "DISABLED";

  sock_to_upper(drv);
  sock_to_upper(epp);
  sock_to_upper(dynepp);
  sock_to_upper(gov);
  sock_to_upper(smt);
  sock_to_upper(plat);

  AodLiveStats aod;
  aod_read_live_telemetry(&aod, &g_aod_cache);
  int aod_online = (aod.wmi_online || g_aod_cache.supported) ? 1 : 0;

  char ppt_s[32] = "N/A", tdc_s[32] = "N/A", edc_s[32] = "N/A",
       mem_s[32] = "N/A";
  if (aod.has_ppt) {
    if (aod.ppt_lim_w > 0.0)
      printf_sn(ppt_s, sizeof(ppt_s), "%.1f W / %.0f W", aod.ppt_val_w,
                aod.ppt_lim_w);
    else
      printf_sn(ppt_s, sizeof(ppt_s), "%.1f W", aod.ppt_val_w);
  }
  if (aod.has_tdc) {
    if (aod.tdc_lim_a > 0.0)
      printf_sn(tdc_s, sizeof(tdc_s), "%.1f A / %.0f A", aod.tdc_val_a,
                aod.tdc_lim_a);
    else
      printf_sn(tdc_s, sizeof(tdc_s), "%.1f A", aod.tdc_val_a);
  }
  if (aod.has_edc) {
    if (aod.edc_lim_a > 0.0)
      printf_sn(edc_s, sizeof(edc_s), "%.1f A / %.0f A", aod.edc_val_a,
                aod.edc_lim_a);
    else
      printf_sn(edc_s, sizeof(edc_s), "%.1f A", aod.edc_val_a);
  }
  if (aod.has_mem)
    printf_sn(mem_s, sizeof(mem_s), "%d MHz", aod.mem_mhz);
  else if (g_aod_cache.has_mem && g_aod_cache.mem_cur_mhz > 0)
    printf_sn(mem_s, sizeof(mem_s), "%d MHz", g_aod_cache.mem_cur_mhz);

  printf_sn(out, out_sz,
            "DAEMON=%s|EBPF=%s|SCHED=%s|VCACHE=%s|BOOST=%s|CCD=%s|CCD0=%s|"
            "CCD1=%s|DRIVER=%s|EPP=%s|DYNEPP=%s|GOV=%s|SMT=%s|PLAT=%s|"
            "CAPS=%llx|AOD_SUP=%d|PPT=%s|TDC=%s|EDC=%s|MEM=%s",
            d_mode, ebpf, sched, vcache, boost, ccd_state, ccd0, ccd1, drv, epp,
            dynepp, gov, smt, plat, (unsigned long long)cfg.capabilities,
            aod_online, ppt_s, tdc_s, edc_s, mem_s);
}

static void sock_format_games_list(char *out, size_t out_sz) {
  out[0] = '\0';
  int fd = open(CONFIG_PATH, O_RDONLY);
  if (fd < 0)
    return;
  char buf[16384];
  size_t total = 0;
  while (total < sizeof(buf) - 1) {
    ssize_t n = read(fd, buf + total, sizeof(buf) - 1 - total);
    if (n <= 0)
      break;
    total += (size_t)n;
  }
  close(fd);
  if (total == 0)
    return;
  buf[total] = '\0';

  int section = 0; /* 1=SYS, 2=USR */
  char *ln = buf;
  while (ln && *ln) {
    char *nxt = strchr(ln, '\n');
    if (nxt)
      *nxt = '\0';
    while (*ln == ' ' || *ln == '\t')
      ln++;
    size_t len = strlen(ln);
    while (len > 0 && (ln[len - 1] == '\r' || ln[len - 1] == ' ' ||
                       ln[len - 1] == '\t'))
      ln[--len] = '\0';

    if (ln[0] == '[') {
      if (strstr(ln, "[GAMES_SYS]"))
        section = 1;
      else if (strstr(ln, "[GAMES_USR]"))
        section = 2;
      else
        section = 0;
    } else if (section != 0 && ln[0] != '\0' && ln[0] != '#' &&
               strncmp(ln, "/*/", 3) != 0 && strchr(ln, '=') == NULL) {
      char entry[96];
      printf_sn(entry, sizeof(entry), "%c:%s\n", (section == 1) ? 'S' : 'U',
                ln);
      strlcat_local(out, entry, out_sz);
    }
    ln = nxt ? nxt + 1 : NULL;
  }
}

static void sock_format_prefcore(char *out, size_t out_sz) {
  char dummy[64] = {0};
  int ccd1 = 0, total = 0;
  if (ccd(dummy, &ccd1, &total) != 0 || total <= 0)
    total = TOTAL_CORES;
  if (total > 64)
    total = 64;

  int ranks[64];
  int any_ok = 0;
  for (int i = 0; i < total; i++) {
    char path[160], rbuf[32] = {0};
    printf_sn(path, sizeof(path),
              "/sys/devices/system/cpu/cpu%d/cpufreq/amd_pstate_prefcore_ranking",
              i);
    sock_read_sysfs(path, rbuf, sizeof(rbuf));
    if (strcmp(rbuf, "N/A") != 0 && rbuf[0]) {
      ranks[i] = atoi(rbuf);
      any_ok = 1;
    } else {
      printf_sn(path, sizeof(path),
                "/sys/devices/system/cpu/cpu%d/acpi_cppc/highest_perf", i);
      sock_read_sysfs(path, rbuf, sizeof(rbuf));
      if (strcmp(rbuf, "N/A") != 0 && rbuf[0]) {
        ranks[i] = atoi(rbuf);
        any_ok = 1;
      } else {
        ranks[i] = -1;
      }
    }
  }

  printf_sn(out, out_sz, "SUPPORTED=%d|COUNT=%d|RANKS=", any_ok, total);
  for (int i = 0; i < total; i++) {
    char tok[16];
    printf_sn(tok, sizeof(tok), "%s%d", (i > 0) ? "," : "", ranks[i]);
    strlcat_local(out, tok, out_sz);
  }
}

int socket_setup(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    syslog(LOG_ERR, "Failed to create AF_UNIX socket: %s", strerror(errno));
    return -1;
  }

  /* Set non-blocking to avoid main loop stalls */
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    syslog(LOG_ERR, "Failed to set non-blocking mode on socket: %s",
           strerror(errno));
    close(fd);
    return -1;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  printf_sn(addr.sun_path, sizeof(addr.sun_path), "%s", IPC_PATH);

  /* Ensure clean state for the socket file */
  unlink(IPC_PATH);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
    syslog(LOG_ERR, "Failed to bind IPC socket to %s: %s", IPC_PATH,
           strerror(errno));
    close(fd);
    return -1;
  }

  if (listen(fd, MAX_CONNECTIONS) < 0) {
    syslog(LOG_ERR, "Failed to listen on IPC socket: %s", strerror(errno));
    close(fd);
    return -1;
  }

  /* Grant 0660 permission to allow the x3d-toggle group to communicate */
  if (chmod(IPC_PATH, 0660) < 0) {
    syslog(LOG_ERR, "Failed to set permissions on IPC socket %s: %s", IPC_PATH,
           strerror(errno));
  }

  syslog(LOG_INFO, "IPC server bound and ready at %s", IPC_PATH);
  return fd;
}

void socket_handle(int server_fd) {
  int client_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);
  if (client_fd < 0)
    return;

  char buf[MAX_IPC_BUFFER_SIZE] = {0};
  if (recv(client_fd, buf, sizeof(buf) - 1, 0) > 0) {
    buf[strcspn(buf, "\r\n")] = 0;

    if (strncmp(buf, "PING", 4) == 0) {
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "HELLO ", 6) == 0) {
      char *endptr = NULL;
      errno = 0;
      long client_ver = strtol(buf + 6, &endptr, 10);
      char resp[64];
      if (endptr != buf + 6 && *endptr == '\0' && errno != ERANGE &&
          client_ver == IPC_PROTOCOL_VERSION) {
        printf_sn(resp, sizeof(resp), "OK %d", IPC_PROTOCOL_VERSION);
      } else {
        printf_sn(resp, sizeof(resp), "ERR_VERSION %d", IPC_PROTOCOL_VERSION);
      }
      send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
    } else if (strncmp(buf, "MODE", 4) == 0) {
      char current[32];
      mode(current, sizeof(current));
      send(client_fd, current, strlen(current), MSG_NOSIGNAL);
    } else if (strncmp(buf, "STATUS", 6) == 0) {
      char snap[1024] = {0};
      sock_format_status(snap, sizeof(snap));
      send(client_fd, snap, strlen(snap), MSG_NOSIGNAL);
    } else if (strncmp(buf, "DAEMON_INFO", 11) == 0) {
      char info[512];
      printf_sn(info, sizeof(info),
                "STATE=%s|OVERRIDE=%d|BPF_ACTIVE=%d|REFRESH_INTERVAL=%.1f|"
                "MASK=%s|CAPS=%llx",
                cfg.daemon_state, active_override, bpf_active ? 1 : 0,
                cfg.refresh_interval, cfg.affinity_mask, cfg.capabilities);
      send(client_fd, info, strlen(info), MSG_NOSIGNAL);
    } else if (strncmp(buf, "GET_CONFIG ", 11) == 0) {
      extern const char *config_get(const char *key);
      const char *val = config_get(buf + 11);
      send(client_fd, val, strlen(val), MSG_NOSIGNAL);
    } else if (strncmp(buf, "TOGGLE", 6) == 0) {
      char current[32];
      const char *target;
      mode(current, sizeof(current));
      target = (strcmp(current, "cache") == 0) ? "frequency" : "cache";
      active_override = (strcmp(target, "cache") == 0) ? 1 : 2;
      cli_set_mode(target);
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_MODE ", 9) == 0) {
      const char *val = buf + 9;
      int matched = 1;
      if (strstr(val, "auto") || strstr(val, "default") ||
          strstr(val, "reset")) {
        active_override = 0;
      } else if (strstr(val, "cache")) {
        active_override = 1;
        cli_set_mode("cache");
        cli_mode_apply(PART_CACHE);
      } else if (strstr(val, "frequency")) {
        active_override = 2;
        cli_set_mode("frequency");
        cli_mode_apply(PART_FREQ);
      } else if (strstr(val, "dual")) {
        active_override = 3;
        cli_set_dual();
        cli_mode_apply(PART_DUAL);
      } else if (strstr(val, "swap")) {
        active_override = 4;
        cli_set_swap();
        cli_mode_apply(PART_DUAL);
      } else
        matched = 0;
      send(client_fd, matched ? "OK" : "ERR", matched ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "GET_TOPOLOGY", 12) == 0) {
      char cache_mask[128] = {0};
      char freq_mask[128] = {0};
      char ccd_tmp[64] = {0};
      int ccd1_start = 0, total_cores = 0;
      affinity_get_topology(cache_mask, freq_mask, sizeof(cache_mask));
      ccd(ccd_tmp, &ccd1_start, &total_cores);
      char resp[512];
      printf_sn(resp, sizeof(resp), "CACHE=%s;FREQ=%s;CCD1=%d;TOTAL=%d",
                cache_mask, freq_mask, ccd1_start, total_cores);
      send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
    } else if (strncmp(buf, "PREFCORE_RANKS", 14) == 0) {
      char resp[512] = {0};
      sock_format_prefcore(resp, sizeof(resp));
      send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_AFFINITY_CONFIG ", 20) == 0) {
      char *cache = buf + 20;
      char *freq = strchr(cache, ' ');
      int ret = ERR_IO;
      if (freq) {
        *freq = '\0';
        freq++;
        ret = affinity_set_masks(cache, freq);
        if (cfg.debug_enable) {
          journal_debug("Affinity masks updated: cache=%s, freq=%s (result=%d)",
                        cache, freq, ret);
        }
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_CPPC ", 9) == 0) {
      const char *val = buf + 9;
      char *endptr = NULL;
      long parsed;
      int ret = ERR_IO;

      errno = 0;
      parsed = strtol(val, &endptr, 10);
      if (endptr != val && errno != ERANGE) {
        while (*endptr == ' ' || *endptr == '\t' || *endptr == '\r' ||
               *endptr == '\n') {
          endptr++;
        }
        if (*endptr == '\0' && parsed >= INT_MIN && parsed <= INT_MAX) {
          ret = cppc_perf((int)parsed);
        }
      }

      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_BOOST ", 10) == 0) {
      int enable = (strstr(buf + 10, "on") != NULL);
      int ret = cppc_boost(enable);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PARK ", 9) == 0) {
      const char *target_str = buf + 9;
      int ret = ERR_SUCCESS;
      if (strcmp(target_str, "ccd1") == 0) {
#if defined(CCD_COUNT) && CCD_COUNT >= 2
        for (int i = 1; i < TOTAL_CORES; i++) {
          if (!cpu_in_cpulist(i, CCD1_CORE_MASK))
            continue;
          int r = cli_set_core(i, 0);
          if (r != ERR_SUCCESS && r != ERR_LOST)
            ret = r;
        }
#else
        ret = ERR_UNSUPPORTED;
#endif
      } else {
        char *endptr = NULL;
        errno = 0;
        long core_long = strtol(target_str, &endptr, 10);
        if (endptr == target_str || *endptr != '\0' || errno == ERANGE ||
            core_long < INT_MIN || core_long > INT_MAX) {
          ret = ERR_LOST;
        } else {
          ret = cli_set_core((int)core_long, 0);
        }
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_UNPARK ", 11) == 0) {
      const char *target_str = buf + 11;
      int ret = ERR_SUCCESS;
      if (strcmp(target_str, "ccd1") == 0 || strcmp(target_str, "all") == 0) {
        ret = cli_set_dual();
      } else {
        char *endptr = NULL;
        errno = 0;
        long core_long = strtol(target_str, &endptr, 10);
        if (endptr == target_str || *endptr != '\0' || errno == ERANGE ||
            core_long < INT_MIN || core_long > INT_MAX) {
          ret = ERR_INVALID_ARGS;
        } else {
          ret = cli_set_core((int)core_long, 1);
        }
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_TDP ", 8) == 0) {
      char *endptr = NULL;
      errno = 0;
      long watts_long = strtol(buf + 8, &endptr, 10);
      int ret = ERR_INVALID_ARGS;
      if (endptr == buf + 8 || *endptr != '\0' || errno == ERANGE ||
          watts_long < INT_MIN || watts_long > INT_MAX) {
        ret = ERR_INVALID_ARGS;
      } else {
        ret = cppc_tdp((int)watts_long);
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_EPP ", 8) == 0) {
      int ret = cppc_epp(buf + 8);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_EPP_CCD ", 12) == 0) {
      char *ccd_str = buf + 12;
      char *val_str = strchr(ccd_str, ' ');
      int ret = ERR_IO;
      if (val_str) {
        char *endptr = NULL;
        errno = 0;
        *val_str = '\0';
        long ccd_long = strtol(ccd_str, &endptr, 10);
        if (endptr == ccd_str || *endptr != '\0' || errno == ERANGE ||
            ccd_long < INT_MIN || ccd_long > INT_MAX) {
          ret = ERR_INVALID_ARGS;
        } else {
          ret = cppc_ccd((int)ccd_long, val_str + 1);
        }
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PSTATE ", 11) == 0) {
      int ret = cppc_pstate(buf + 11);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_GOVERNOR ", 13) == 0) {
      int ret = cppc_governor(buf + 13);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PREFCORE ", 13) == 0) {
      int enable =
          (strstr(buf + 13, "on") != NULL || strstr(buf + 13, "1") != NULL);
      int ret = cppc_prefcore(enable);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_DAEMON ", 11) == 0) {
      char *val = buf + 11;
      if (strcmp(val, "manual") == 0 || strcmp(val, "auto") == 0 ||
          strcmp(val, "default") == 0) {
        config_update("DAEMON_STATE", val);
        printf_sn(cfg.daemon_state, sizeof(cfg.daemon_state), "%s", val);

        if (strcmp(val, "auto") == 0) {
          pid_t rpid = fork();
          if (rpid == 0) {
            char *args[] = {
                (char *)"/bin/sh",
                (char *)"/usr/lib/x3d-toggle/scripts/tools/reset.sh", NULL};
            char *envp[] = {(char *)"X3D_EXEC=1", NULL};
            execve(args[0], args, envp);
            _exit(EXIT_FAILURE);
          } else if (rpid > 0) {
            int rst;
            (void)waitpid(rpid, &rst, 0);
          } else {
            int fork_errno = errno;
            syslog(LOG_ERR, "fork() failed for reset.sh: %s",
                   strerror(fork_errno));
            send(client_fd, "ERR", 3, MSG_NOSIGNAL);
            close(client_fd);
            return;
          }
        }

        if (active_override != 0 &&
            (strcmp(val, "auto") == 0 || strcmp(val, "default") == 0))
          active_override = 0;
        poll_now = 1;
        send(client_fd, "OK", 2, MSG_NOSIGNAL);
      } else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_CONFIG ", 11) == 0) {
      char *kv = buf + 11;
      char *v = strchr(kv, ' ');
      if (v) {
        *v = '\0';
        v++;
        config_update(kv, v);
        config_load(&cfg);
        if (strcmp(kv, "EBPF_ENABLE") == 0) {
          if (cfg.ebpf_enable && !bpf_active)
            bpf_active = bpf_init();
          else if (!cfg.ebpf_enable && bpf_active) {
            bpf_cleanup();
            bpf_active = false;
          }
        } else if (strcmp(kv, "DAEMON_STATE") == 0) {
          if (active_override != 0 &&
              (strcmp(v, "auto") == 0 || strcmp(v, "default") == 0)) {
            active_override = 0;
          }
        }
        if (strcmp(kv, "FALLBACK_PROFILE") == 0 ||
            strcmp(kv, "DAEMON_STATE") == 0 ||
            strcmp(kv, "LOAD_THRESHOLD") == 0 ||
            strcmp(kv, "DETECTION_LEVEL") == 0) {
          poll_now = 1;
        }
        if (cfg.debug_enable) {
          journal_debug("Configuration Update: %s = %s", kv, v);
        }
        send(client_fd, "OK", 2, MSG_NOSIGNAL);
      } else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_WRAPPER ", 12) == 0) {
      extern volatile int wrapper_lock;
      int enable = atoi(buf + 12);

      wrapper_lock = enable;

      if (enable) {
        cli_set_mode("cache");
        active_override = 1;
      } else {
        cli_set_mode("frequency");
        active_override = 0;
      }
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "DAEMON_DISABLE", 14) == 0) {
      active_keep = 0;
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "CONFIG_SYNC", 11) == 0) {
      config_load(&cfg);
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "GAMES_LIST", 10) == 0) {
      char list_buf[4096] = {0};
      sock_format_games_list(list_buf, sizeof(list_buf));
      if (list_buf[0] == '\0')
        printf_sn(list_buf, sizeof(list_buf), "EMPTY");
      send(client_fd, list_buf, strlen(list_buf), MSG_NOSIGNAL);
    } else if (strncmp(buf, "GAME_ADD ", 9) == 0) {
      int ret = game_add(buf + 9);
      if (ret == ERR_SUCCESS)
        config_load(&cfg);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "GAME_REMOVE ", 12) == 0) {
      int ret = game_remove(buf + 12);
      if (ret == ERR_SUCCESS)
        config_load(&cfg);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "ADVANCED_GET ", 13) == 0) {
      const char *key = buf + 13;
      char path[256] = {0};
      char val[128] = {0};
      int ret = -1;

      if (strcmp(key, "kernel.sched_bore") == 0)
        printf_sn(path, 256, "/proc/sys/kernel/sched_bore");
      else if (strcmp(key, "kernel.sched_burst_penalty_offset") == 0 ||
               strcmp(key, "kernel.sched_bit_shift") == 0) {
        if (access("/proc/sys/kernel/sched_burst_penalty_offset", F_OK) == 0)
          printf_sn(path, 256, "/proc/sys/kernel/sched_burst_penalty_offset");
        else
          printf_sn(path, 256, "/proc/sys/kernel/sched_bit_shift");
      } else if (strcmp(key, "kernel.sched_burst_inherit_type") == 0 ||
                 strcmp(key, "kernel.sched_burst_fork_at_init") == 0) {
        if (access("/proc/sys/kernel/sched_burst_inherit_type", F_OK) == 0)
          printf_sn(path, 256, "/proc/sys/kernel/sched_burst_inherit_type");
        else
          printf_sn(path, 256, "/proc/sys/kernel/sched_burst_fork_at_init");
      } else if (strcmp(key, "kernel.sched_cfs_bandwidth_slice_us") == 0)
        printf_sn(path, 256, "/proc/sys/kernel/sched_cfs_bandwidth_slice_us");
      else if (strcmp(key, "vm.max_map_count") == 0)
        printf_sn(path, 256, "/proc/sys/vm/max_map_count");
      else if (strcmp(key, "kernel.split_lock_mitigate") == 0)
        printf_sn(path, 256, "/proc/sys/kernel/split_lock_mitigate");
      else if (strcmp(key, "kernel.nmi_watchdog") == 0)
        printf_sn(path, 256, "/proc/sys/kernel/nmi_watchdog");
      else if (strcmp(key, "net.core.default_qdisc") == 0)
        printf_sn(path, 256, "/proc/sys/net/core/default_qdisc");
      else if (strcmp(key, "net.ipv4.tcp_fastopen") == 0)
        printf_sn(path, 256, "/proc/sys/net/ipv4/tcp_fastopen");
      else if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0)
        printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/rp_filter");
      else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") == 0)
        printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/accept_source_route");
      else if (strcmp(key, "target_thp_mode") == 0)
        printf_sn(path, 256, "/sys/kernel/mm/transparent_hugepage/enabled");
      else if (strcmp(key, "net.ipv4.tcp_congestion_control") == 0)
        printf_sn(path, 256, "/proc/sys/net/ipv4/tcp_congestion_control");
      else if (strcmp(key, "net.ipv4.tcp_available_congestion_control") == 0)
        printf_sn(path, 256,
                  "/proc/sys/net/ipv4/tcp_available_congestion_control");
      else if (strcmp(key, "pcie.aspm_policy") == 0)
        printf_sn(path, 256, "/sys/module/pcie_aspm/parameters/policy");

      if (path[0]) {
        ret = sysctl_read(path, val, sizeof(val));
      }

      if (ret == 0)
        send(client_fd, val, strlen(val), MSG_NOSIGNAL);
      else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "ADVANCED_SET ", 13) == 0) {
      char *key = buf + 13;
      char *val = strchr(key, ' ');
      int ret = -1;
      if (val) {
        *val = '\0';
        val++;
        char path[256] = {0};

        if (strcmp(key, "kernel.sched_bore") == 0) {
          printf_sn(path, 256, "/proc/sys/kernel/sched_bore");
          cfg.sched_bore = atoi(val);
        } else if (strcmp(key, "kernel.sched_burst_penalty_offset") == 0 ||
                   strcmp(key, "kernel.sched_bit_shift") == 0) {
          if (access("/proc/sys/kernel/sched_burst_penalty_offset", F_OK) == 0)
            printf_sn(path, 256, "/proc/sys/kernel/sched_burst_penalty_offset");
          else
            printf_sn(path, 256, "/proc/sys/kernel/sched_bit_shift");
          cfg.sched_bit_shift = atoi(val);
        } else if (strcmp(key, "kernel.sched_burst_inherit_type") == 0 ||
                   strcmp(key, "kernel.sched_burst_fork_at_init") == 0) {
          if (access("/proc/sys/kernel/sched_burst_inherit_type", F_OK) == 0)
            printf_sn(path, 256, "/proc/sys/kernel/sched_burst_inherit_type");
          else
            printf_sn(path, 256, "/proc/sys/kernel/sched_burst_fork_at_init");
          cfg.sched_burst_fork = atoi(val);
        } else if (strcmp(key, "kernel.sched_cfs_bandwidth_slice_us") == 0) {
          printf_sn(path, 256, "/proc/sys/kernel/sched_cfs_bandwidth_slice_us");
          cfg.sched_slice_us = atoi(val);
        } else if (strcmp(key, "vm.max_map_count") == 0) {
          printf_sn(path, 256, "/proc/sys/vm/max_map_count");
          cfg.vm_max_map = atoi(val);
        } else if (strcmp(key, "kernel.split_lock_mitigate") == 0) {
          printf_sn(path, 256, "/proc/sys/kernel/split_lock_mitigate");
          cfg.split_lock_detect = atoi(val);
        } else if (strcmp(key, "kernel.nmi_watchdog") == 0) {
          printf_sn(path, 256, "/proc/sys/kernel/nmi_watchdog");
          cfg.nmi_watchdog = atoi(val);
        } else if (strcmp(key, "net.core.default_qdisc") == 0) {
          printf_sn(path, 256, "/proc/sys/net/core/default_qdisc");
          printf_sn(cfg.net_qdisc, 31, "%s", val);
        } else if (strcmp(key, "net.ipv4.tcp_fastopen") == 0) {
          printf_sn(path, 256, "/proc/sys/net/ipv4/tcp_fastopen");
          cfg.net_fastopen = atoi(val);
        } else if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0) {
          printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/rp_filter");
          cfg.net_rp_filter = atoi(val);
        } else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") == 0) {
          printf_sn(path, 256,
                    "/proc/sys/net/ipv4/conf/all/accept_source_route");
          cfg.net_source_route = atoi(val);
        } else if (strcmp(key, "target_thp_mode") == 0) {
          printf_sn(path, 256, "/sys/kernel/mm/transparent_hugepage/enabled");
          printf_sn(cfg.thp_mode, 15, "%s", val);
        }

        if (path[0]) {
          ret = sysctl_write(path, val);
          if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0) {
            sysctl_write("/proc/sys/net/ipv4/conf/default/rp_filter", val);
          } else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") ==
                     0) {
            sysctl_write("/proc/sys/net/ipv4/conf/default/accept_source_route",
                         val);
          }
          if (ret == 0) {
            /* Persist change to daemon.conf */
            char ckey[64] = {0};
            if (strcmp(key, "kernel.sched_bore") == 0)
              printf_sn(ckey, sizeof(ckey), "SCHED_BORE");
            else if (strcmp(key, "kernel.sched_burst_penalty_offset") == 0 ||
                     strcmp(key, "kernel.sched_bit_shift") == 0)
              printf_sn(ckey, sizeof(ckey), "SCHED_BIT_SHIFT");
            else if (strcmp(key, "kernel.sched_burst_inherit_type") == 0 ||
                     strcmp(key, "kernel.sched_burst_fork_at_init") == 0)
              printf_sn(ckey, sizeof(ckey), "SCHED_BURST_FORK");
            else if (strcmp(key, "kernel.sched_cfs_bandwidth_slice_us") == 0)
              printf_sn(ckey, sizeof(ckey), "SCHED_SLICE_US");
            else if (strcmp(key, "vm.max_map_count") == 0)
              printf_sn(ckey, sizeof(ckey), "VM_MAX_MAP");
            else if (strcmp(key, "kernel.split_lock_mitigate") == 0)
              printf_sn(ckey, sizeof(ckey), "SPLIT_LOCK_DETECT");
            else if (strcmp(key, "kernel.nmi_watchdog") == 0)
              printf_sn(ckey, sizeof(ckey), "NMI_WATCHDOG");
            else if (strcmp(key, "net.core.default_qdisc") == 0)
              printf_sn(ckey, sizeof(ckey), "NET_QDISC");
            else if (strcmp(key, "net.ipv4.tcp_fastopen") == 0)
              printf_sn(ckey, sizeof(ckey), "NET_FASTOPEN");
            else if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0)
              printf_sn(ckey, sizeof(ckey), "NET_RP_FILTER");
            else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") == 0)
              printf_sn(ckey, sizeof(ckey), "NET_SOURCE_ROUTE");
            else if (strcmp(key, "target_thp_mode") == 0)
              printf_sn(ckey, sizeof(ckey), "THP_MODE");

            config_update(ckey, val);
          }
        }
      }
      send(client_fd, (ret == 0) ? "OK" : "ERR", (ret == 0) ? 2 : 3,
           MSG_NOSIGNAL);
    } else if (strncmp(buf, "IRQ_STATUS", 10) == 0) {
      char resp[512] = {0};
      if (irq_format_summary(resp, sizeof(resp)) == ERR_SUCCESS)
        send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
      else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "IRQ_BIND", 8) == 0) {
      extern int cli_irq_bind(int argc, char *argv[]);
      int ret = cli_irq_bind(0, NULL);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "IRQ_RESET", 9) == 0) {
      extern int cli_irq_reset(int argc, char *argv[]);
      int ret = cli_irq_reset(0, NULL);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "AOD_LIVE", 8) == 0) {
      sock_ensure_aod();
      AodLiveStats aod;
      aod_read_live_telemetry(&aod, &g_aod_cache);
      int wmi_on = (aod.wmi_online || g_aod_cache.supported) ? 1 : 0;
      int mem_ok = aod.has_mem;
      int mem_v = aod.mem_mhz;
      if (!mem_ok && g_aod_cache.has_mem && g_aod_cache.mem_cur_mhz > 0) {
        mem_ok = 1;
        mem_v = g_aod_cache.mem_cur_mhz;
      }
      char resp[256] = {0};
      printf_sn(resp, sizeof(resp),
                "WMI=%d|PPT=%d,%.1f,%.0f|TDC=%d,%.1f,%.0f|EDC=%d,%.1f,%.0f|"
                "MEM=%d,%d",
                wmi_on, aod.has_ppt, aod.ppt_val_w, aod.ppt_lim_w, aod.has_tdc,
                aod.tdc_val_a, aod.tdc_lim_a, aod.has_edc, aod.edc_val_a,
                aod.edc_lim_a, mem_ok, mem_v);
      send(client_fd, resp, strlen(resp), MSG_NOSIGNAL);
    } else if (strncmp(buf, "AOD_INFO", 8) == 0) {
      /* Always probe *_min and *_max fresh from sysfs rather than serving
       * cached bounds, since UEFI/AGESA/OC mode changes can shift limits. */
      aod_probe_state(&g_aod_cache);
      g_aod_probed = 1;
      char aod_resp[512] = {0};
      aod_format_ipc_info(&g_aod_cache, aod_resp, sizeof(aod_resp));
      send(client_fd, aod_resp, strlen(aod_resp), MSG_NOSIGNAL);
    } else if (strncmp(buf, "AOD_APPLY ", 10) == 0) {
      const char *p = buf + 10;
      char *end = NULL;
      long ppt = strtol(p, &end, 10);
      long tdc = (end && *end == ' ') ? strtol(end + 1, &end, 10) : -1;
      long edc = (end && *end == ' ') ? strtol(end + 1, &end, 10) : -1;
      long mem = (end && *end == ' ') ? strtol(end + 1, &end, 10) : -1;
      int ret = ERR_INVALID_ARGS;
      if (ppt >= 0 && tdc >= 0 && edc >= 0 && mem >= 0 &&
          (ppt > 0 || tdc > 0 || edc > 0 || mem > 0)) {
        ret = aod_apply_limits((int)ppt, (int)tdc, (int)edc, (int)mem);
        if (ret == ERR_SUCCESS) {
          char vbuf[32];
          if (ppt > 0) {
            printf_sn(vbuf, sizeof(vbuf), "%ld", ppt);
            config_update("AOD_PPT_W", vbuf);
          }
          if (tdc > 0) {
            printf_sn(vbuf, sizeof(vbuf), "%ld", tdc);
            config_update("AOD_TDC_A", vbuf);
          }
          if (edc > 0) {
            printf_sn(vbuf, sizeof(vbuf), "%ld", edc);
            config_update("AOD_EDC_A", vbuf);
          }
          if (mem > 0) {
            printf_sn(vbuf, sizeof(vbuf), "%ld", mem);
            config_update("AOD_MEM_MHZ", vbuf);
          }
        }
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "AOD_SET_SMT ", 12) == 0) {
      int enable = (atoi(buf + 12) != 0) ? 1 : 0;
      int ret = aod_set_smt(enable);
      if (ret == ERR_SUCCESS) {
        if (g_aod_probed)
          g_aod_cache.smt_cur = enable;
        config_update("AOD_SMT_ENABLE", enable ? "1" : "0");
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR",
           (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else {
      send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    }
  }
  close(client_fd);
}

/* end of SOCKET.C */
