/* Socket Server for the X3D Toggle Project
 *
 * `socket.c`
 *
 * Handles Daemon-side UNIX Domain Socket creation and listens for CLI commands.
 * Employs a non-blocking select() loop for graceful termination.
 */

#include "libc.h"
#include "daemon/bpf/bpf-user.h"
#include "../build/config.h"
#include "modes.h"
#include "cppc.h"
#include "error.h"
#include "ipc.h"
#include "../build/xui.h"

extern volatile sig_atomic_t active_keep;
extern volatile int active_override;
extern DaemonConfig cfg;
extern bool bpf_active;

int socket_setup(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    syslog(LOG_ERR, "Failed to create AF_UNIX socket: %s", strerror(errno));
    return -1;
  }

  /* Set non-blocking to avoid main loop stalls */
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  scat(addr.sun_path, IPC_PATH, sizeof(addr.sun_path));

  /* Ensure clean state for the socket file */
  unlink(IPC_PATH);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
    syslog(LOG_ERR, "Failed to bind IPC socket to %s: %s", IPC_PATH, strerror(errno));
    close(fd);
    return -1;
  }

  if (listen(fd, MAX_CONNECTIONS) < 0) {
    syslog(LOG_ERR, "Failed to listen on IPC socket: %s", strerror(errno));
    close(fd);
    return -1;
  }

  /* Grant 0660 permission to allow the x3d-toggle group to communicate */
  chmod(IPC_PATH, 0660);

  syslog(LOG_INFO, "IPC server bound and ready at %s", IPC_PATH);
  return fd;
}

void socket_handle(int server_fd) {
  int client_fd = accept4(server_fd, NULL, NULL, SOCK_CLOEXEC);
  if (client_fd < 0)
    return;

  char buf[512] = {0};
  if (recv(client_fd, buf, sizeof(buf) - 1, 0) > 0) {
    buf[strcspn(buf, "\r\n")] = 0;

    if (strncmp(buf, "PING", 4) == 0) {
      send(client_fd, "OK", 2, MSG_NOSIGNAL);
    } else if (strncmp(buf, "MODE", 8) == 0) {
      char current[32];
      mode(current, sizeof(current));
      send(client_fd, current, strlen(current), MSG_NOSIGNAL);
    } else if (strncmp(buf, "DAEMON_INFO", 11) == 0) {
      char info[128];
      printf_sn(info, sizeof(info), "STATE=%s;OVERRIDE=%d;BPF_ACTIVE=%d;REFRESH_INTERVAL=%.1f",
                cfg.daemon_state, active_override, bpf_active ? 1 : 0, cfg.refresh_interval);
      send(client_fd, info, strlen(info), MSG_NOSIGNAL);
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
      if (strstr(val, "auto") || strstr(val, "default") || strstr(val, "reset")) {
        active_override = 0;
      } else if (strstr(val, "cache")) {
        active_override = 1;
      } else if (strstr(val, "frequency")) {
        active_override = 2;
      } else if (strstr(val, "dual")) {
        active_override = 3;
      } else if (strstr(val, "swap")) {
        active_override = 4;
      } else
        matched = 0;
      send(client_fd, matched ? "OK" : "ERR", matched ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_CPPC ", 9) == 0) {
      int ret = cppc_perf(atoi(buf + 9));
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_BOOST ", 10) == 0) {
      int enable = (strstr(buf + 10, "on") != NULL);
      int ret = cppc_boost(enable);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PARK ", 9) == 0) {
      char *target_str = buf + 9;
      int ret = ERR_SUCCESS;
      if (strcmp(target_str, "ccd1") == 0) {
        for (int i = 1; i < 128; i++) {
          int r = cli_set_core(i, 0);
          if (r != ERR_SUCCESS && r != ERR_LOST)
            ret = r;
        }
      } else {
        ret = cli_set_core(atoi(target_str), 0);
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_UNPARK ", 11) == 0) {
      char *target_str = buf + 11;
      int ret = ERR_SUCCESS;
      if (strcmp(target_str, "ccd1") == 0 || strcmp(target_str, "all") == 0) {
        ret = cli_set_dual();
      } else {
        ret = cli_set_core(atoi(target_str), 1);
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_TDP ", 8) == 0) {
      int watts = atoi(buf + 8);
      int ret = cppc_tdp(watts);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_EPP ", 8) == 0) {
      int ret = cppc_epp(buf + 8);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_EPP_CCD ", 12) == 0) {
      char *ccd_str = buf + 12;
      char *val_str = strchr(ccd_str, ' ');
      int ret = ERR_IO;
      if (val_str) {
        *val_str = '\0';
        ret = cppc_ccd(atoi(ccd_str), val_str + 1);
      }
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PSTATE ", 11) == 0) {
      int ret = cppc_pstate(buf + 11);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_GOVERNOR ", 13) == 0) {
      int ret = cppc_governor(buf + 13);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_PREFCORE ", 13) == 0) {
      int enable = (strstr(buf + 13, "on") != NULL || strstr(buf + 13, "1") != NULL);
      int ret = cppc_prefcore(enable);
      send(client_fd, (ret == ERR_SUCCESS) ? "OK" : "ERR", (ret == ERR_SUCCESS) ? 2 : 3, MSG_NOSIGNAL);
    } else if (strncmp(buf, "SET_DAEMON ", 11) == 0) {
      char *val = buf + 11;
      if (strcmp(val, "manual") == 0 || strcmp(val, "auto") == 0 || strcmp(val, "default") == 0) {
        config_update("DAEMON_STATE", val);
        scat(cfg.daemon_state, val, sizeof(cfg.daemon_state));

        if (strcmp(val, "auto") == 0) {
          system("X3D_EXEC=1 sh /usr/lib/x3d-toggle/scripts/tools/reset.sh");
        }

        if (active_override != 0 && (strcmp(val, "auto") == 0 || strcmp(val, "default") == 0))
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
          scat(cfg.server_address, v, sizeof(cfg.server_address));
        else if (strcmp(kv, "EBPF_ENABLE") == 0) {
          cfg.ebpf_enable = atoi(v);
          if (cfg.ebpf_enable && !bpf_active)
            bpf_init();
          else if (!cfg.ebpf_enable && bpf_active) {
            bpf_active = false;
            bpf_cleanup();
          }
        }
        send(client_fd, "OK", 2, MSG_NOSIGNAL);
      } else
        send(client_fd, "ERR", 3, MSG_NOSIGNAL);
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
    } else {
      send(client_fd, "ERR", 3, MSG_NOSIGNAL);
    }
  }
  close(client_fd);
}

/* end of SOCKET.C */
