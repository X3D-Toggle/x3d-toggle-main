/* Socket Server for the X3D Toggle Project
 * `socket.c`
 * Handles Daemon-side UNIX Domain Socket creation and listens for CLI commands.
 * Employs a non-blocking select() loop for graceful termination.
 */

#include "../build/ccd.h"
#include "../build/config.h"
#include "../build/xui.h"
#include "cppc.h"
#include "daemon/bpf/bpf-user.h"
#include "error.h"
#include "ipc.h"
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
    } else if (strncmp(buf, "MODE", 4) == 0) {
      char current[32];
      mode(current, sizeof(current));
      send(client_fd, current, strlen(current), MSG_NOSIGNAL);
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
      char current[32], *target;
      mode(current, sizeof(current));
      target = (strcmp(current, "cache") == 0) ? "frequency" : "cache";
      active_override = (strcmp(target, "cache") == 0) ? 1 : 2;
      cli_set_mode(target);
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_MODE ", 9) == 0) {
      char *val = buf + 9;
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
      affinity_get_topology(cache_mask, freq_mask, sizeof(cache_mask));
      char resp[512];
      printf_sn(resp, sizeof(resp), "CACHE=%s;FREQ=%s", cache_mask, freq_mask);
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
      char *val = buf + 9;
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
      char *target_str = buf + 9;
      int ret = ERR_SUCCESS;
      if (strcmp(target_str, "ccd1") == 0) {
#ifdef CCD1_START
        const int ccd1_start = CCD1_START;
#else
        const int ccd1_start = TOTAL_CORES / 2;
#endif
        for (int i = ccd1_start; i < TOTAL_CORES; i++) {
          int r = cli_set_core(i, 0);
          if (r != ERR_SUCCESS && r != ERR_LOST)
            ret = r;
        }
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
      char *target_str = buf + 11;
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
        send(client_fd, "OK", 2, MSG_NOSIGNAL);
      } else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_CONFIG ", 11) == 0) {
      char *kv = buf + 11;
      char *v = strchr(kv, ' ');
      if (v) {
        *v = '\0';
        config_update(kv, ++v);
        if (strcmp(kv, "DETECTION_LEVEL") == 0)
          cfg.detection_level = atoi(v);
        else if (strcmp(kv, "POLLING_INTERVAL") == 0)
          cfg.polling_interval = atoi(v);
        else if (strcmp(kv, "LOAD_THRESHOLD") == 0)
          cfg.load_threshold = atof(v);
        else if (strcmp(kv, "SERVER_ADDRESS") == 0)
          printf_sn(cfg.server_address, sizeof(cfg.server_address), "%s", v);
        else if (strcmp(kv, "EBPF_ENABLE") == 0) {
          cfg.ebpf_enable = atoi(v);
          if (cfg.ebpf_enable && !bpf_active)
            bpf_active = bpf_init();
          else if (!cfg.ebpf_enable && bpf_active) {
            bpf_cleanup();
            bpf_active = false;
          }
        } else if (strcmp(kv, "AFFINITY_MASK") == 0) {
          printf_sn(cfg.affinity_mask, sizeof(cfg.affinity_mask), "%s", v);
        } else if (strcmp(kv, "AFFINITY_FREQ_MASK") == 0) {
          printf_sn(cfg.affinity_freq_mask, sizeof(cfg.affinity_freq_mask), "%s", v);
        } else if (strcmp(kv, "DEV_ENABLE") == 0) {
          cfg.dev_enable = atoi(v);
        } else if (strcmp(kv, "DEBUG_ENABLE") == 0) {
          cfg.debug_enable = atoi(v);
        } else if (strcmp(kv, "AFFINITY_LEVEL") == 0) {
          cfg.affinity_level = atoi(v);
        } else if (strcmp(kv, "REFRESH_INTERVAL") == 0) {
          cfg.refresh_interval = atof(v);
        } else if (strcmp(kv, "FALLBACK_PROFILE") == 0) {
          printf_sn(cfg.fallback_profile, sizeof(cfg.fallback_profile), "%s", v);
        }
        /* Advanced Tunables */
        else if (strcmp(kv, "SCHED_BORE") == 0) cfg.sched_bore = atoi(v);
        else if (strcmp(kv, "SCHED_BIT_SHIFT") == 0) cfg.sched_bit_shift = atoi(v);
        else if (strcmp(kv, "SCHED_BURST_FORK") == 0) cfg.sched_burst_fork = atoi(v);
        else if (strcmp(kv, "SCHED_SLICE_US") == 0) cfg.sched_slice_us = atoi(v);
        else if (strcmp(kv, "VM_MAX_MAP") == 0) cfg.vm_max_map = atoi(v);
        else if (strcmp(kv, "SPLIT_LOCK_DETECT") == 0) cfg.split_lock_detect = atoi(v);
        else if (strcmp(kv, "NMI_WATCHDOG") == 0) cfg.nmi_watchdog = atoi(v);
        else if (strcmp(kv, "THP_MODE") == 0) printf_sn(cfg.thp_mode, sizeof(cfg.thp_mode), "%s", v);
        /* Networking */
        else if (strcmp(kv, "NET_QDISC") == 0) printf_sn(cfg.net_qdisc, sizeof(cfg.net_qdisc), "%s", v);
        else if (strcmp(kv, "NET_FASTOPEN") == 0) cfg.net_fastopen = atoi(v);
        else if (strcmp(kv, "NET_RP_FILTER") == 0) cfg.net_rp_filter = atoi(v);
        else if (strcmp(kv, "NET_SOURCE_ROUTE") == 0) cfg.net_source_route = atoi(v);
        /* IRQ Subsystem */
        else if (strcmp(kv, "IRQ_ENABLE") == 0) cfg.irq_enable = atoi(v);
        else if (strcmp(kv, "IRQ_GPU") == 0) cfg.irq_gpu = atoi(v);
        else if (strcmp(kv, "IRQ_NVME") == 0) cfg.irq_nvme = atoi(v);
        else if (strcmp(kv, "IRQ_USB") == 0) cfg.irq_usb = atoi(v);
        else if (strcmp(kv, "IRQ_NIC") == 0) cfg.irq_nic = atoi(v);
        else if (strcmp(kv, "IRQ_AUDIO") == 0) cfg.irq_audio = atoi(v);
        else if (strcmp(kv, "IRQ_COALESCE") == 0) cfg.irq_coalesce = atoi(v);
        else if (strcmp(kv, "IRQ_WATCH") == 0) cfg.irq_watch = atoi(v);
        /* Developer */
        else if (strcmp(kv, "ADVANCED_CONFIG_ENABLE") == 0) cfg.advanced_config_enable = atoi(v);
        else if (strcmp(kv, "FORCE_UNSUPPORTED_DISPLAY") == 0) cfg.force_unsupported_display = atoi(v);
        journal_info(DAEMON_TRANSIT, "Configuration Update: %s -> %s", kv, v);
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
      char list_buf[512] = "";
      printf_sn(list_buf, sizeof(list_buf), "GAMES_SYNCED");
      send(client_fd, list_buf, strlen(list_buf), MSG_NOSIGNAL);
    } else if (strncmp(buf, "ADVANCED_GET ", 13) == 0) {
      char *key = buf + 13;
      char path[256] = {0};
      char val[128] = {0};
      int ret = -1;
      
      if (strcmp(key, "kernel.sched_bore") == 0) printf_sn(path, 256, "/proc/sys/kernel/sched_bore");
      else if (strcmp(key, "kernel.sched_bit_shift") == 0) printf_sn(path, 256, "/proc/sys/kernel/sched_bit_shift");
      else if (strcmp(key, "kernel.sched_burst_fork_at_init") == 0) printf_sn(path, 256, "/proc/sys/kernel/sched_burst_fork_at_init");
      else if (strcmp(key, "kernel.sched_cfs_bandwidth_slice_us") == 0) printf_sn(path, 256, "/proc/sys/kernel/sched_cfs_bandwidth_slice_us");
      else if (strcmp(key, "vm.max_map_count") == 0) printf_sn(path, 256, "/proc/sys/vm/max_map_count");
      else if (strcmp(key, "kernel.split_lock_mitigate") == 0) printf_sn(path, 256, "/proc/sys/kernel/split_lock_mitigate");
      else if (strcmp(key, "kernel.nmi_watchdog") == 0) printf_sn(path, 256, "/proc/sys/kernel/nmi_watchdog");
      else if (strcmp(key, "net.core.default_qdisc") == 0) printf_sn(path, 256, "/proc/sys/net/core/default_qdisc");
      else if (strcmp(key, "net.ipv4.tcp_fastopen") == 0) printf_sn(path, 256, "/proc/sys/net/ipv4/tcp_fastopen");
      else if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0) printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/rp_filter");
      else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") == 0) printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/accept_source_route");
      else if (strcmp(key, "target_thp_mode") == 0) printf_sn(path, 256, "/sys/kernel/mm/transparent_hugepage/enabled");

      if (path[0]) {
          ret = sysctl_read(path, val, sizeof(val));
      }
      
      if (ret == 0) send(client_fd, val, strlen(val), MSG_NOSIGNAL);
      else send(client_fd, "ERR", 3, MSG_NOSIGNAL);
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
          } else if (strcmp(key, "kernel.sched_bit_shift") == 0) {
              printf_sn(path, 256, "/proc/sys/kernel/sched_bit_shift");
              cfg.sched_bit_shift = atoi(val);
          } else if (strcmp(key, "kernel.sched_burst_fork_at_init") == 0) {
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
              printf_sn(path, 256, "/proc/sys/net/ipv4/conf/all/accept_source_route");
              cfg.net_source_route = atoi(val);
          } else if (strcmp(key, "target_thp_mode") == 0) {
              printf_sn(path, 256, "/sys/kernel/mm/transparent_hugepage/enabled");
              printf_sn(cfg.thp_mode, 15, "%s", val);
          }
          
          if (path[0]) {
              ret = sysctl_write(path, val);
              if (ret == 0) {
                  /* Persist change to daemon.conf */
                  char ckey[64];
                  if (strcmp(key, "kernel.sched_bore") == 0) strcpy(ckey, "SCHED_BORE");
                  else if (strcmp(key, "kernel.sched_bit_shift") == 0) strcpy(ckey, "SCHED_BIT_SHIFT");
                  else if (strcmp(key, "kernel.sched_burst_fork_at_init") == 0) strcpy(ckey, "SCHED_BURST_FORK");
                  else if (strcmp(key, "kernel.sched_cfs_bandwidth_slice_us") == 0) strcpy(ckey, "SCHED_SLICE_US");
                  else if (strcmp(key, "vm.max_map_count") == 0) strcpy(ckey, "VM_MAX_MAP");
                  else if (strcmp(key, "kernel.split_lock_mitigate") == 0) strcpy(ckey, "SPLIT_LOCK_DETECT");
                  else if (strcmp(key, "kernel.nmi_watchdog") == 0) strcpy(ckey, "NMI_WATCHDOG");
                  else if (strcmp(key, "net.core.default_qdisc") == 0) strcpy(ckey, "NET_QDISC");
                  else if (strcmp(key, "net.ipv4.tcp_fastopen") == 0) strcpy(ckey, "NET_FASTOPEN");
                  else if (strcmp(key, "net.ipv4.conf.all.rp_filter") == 0) strcpy(ckey, "NET_RP_FILTER");
                  else if (strcmp(key, "net.ipv4.conf.all.accept_source_route") == 0) strcpy(ckey, "NET_SOURCE_ROUTE");
                  else if (strcmp(key, "target_thp_mode") == 0) strcpy(ckey, "THP_MODE");
                  
                  config_update(ckey, val);
              }
          }
      }
      send(client_fd, (ret == 0) ? "OK" : "ERR", (ret == 0) ? 2 : 3, MSG_NOSIGNAL);
    } else {
      send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    }
  }
  close(client_fd);
}

/* end of SOCKET.C */
