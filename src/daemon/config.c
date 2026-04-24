/* Configuration/Ruleset Management for the X3D Toggle Project
 * `config.c` - Backend Consolidation
 * Handles both the CLI handlers and the backend implementation for 
 * configuration loading, updating, and logic-generation. 
 */

#ifndef USR_LIBS
#define USR_LIBS "/usr/lib/x3d-toggle"
#endif

#ifndef DIR_BIN
#define DIR_BIN "/etc/x3d-toggle.d"
#endif

#include "config.h" 
#include "xui.h"
#include "cli.h"
#include "error.h"
#include "games.h"
#include "ipc.h"
#include "libc.h"

static int config_write(int argc, char *argv[], const char *ipc_command,
                        const char *config_key, const char *config_value,
                        const char *user_message) {

  int ipc_res = socket_send(ipc_command, NULL, 0);
  if (ipc_res == 0) {
    printf_string("✅ %s (Active daemon synced)", user_message);
    return 0;
  }

  if (geteuid() != 0) {
    journal_warn(ERR_VIP);
    pid_t pid = fork();
    if (pid == 0) {
      char *new_argv[32]; // Safety limit for CLI args
      int idx = 0;
      new_argv[idx++] = (char *)"/usr/bin/pkexec";
      new_argv[idx++] = (char *)"/usr/bin/x3d-toggle";
      for (int i = 1; i < argc && idx < 31; i++) {
        new_argv[idx++] = argv[i];
      }
      new_argv[idx] = NULL;
      char *env[] = {NULL};
      execve(new_argv[0], new_argv, env);
      _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    return (status == 0) ? 0 : 1;
  }

  if (config_key != NULL && config_value != NULL) {
    config_update(config_key, config_value);
  } else if (strncmp(ipc_command, "GAME_ADD ", 9) == 0) {
    game_add(ipc_command + 9);
  } else if (strncmp(ipc_command, "GAME_REMOVE ", 12) == 0) {
    game_remove(ipc_command + 12);
  }

  printf_string("✅ %s (Direct file write - Daemon offline)", user_message);
  return 0;
}

int cli_config_interval(int argc, char *argv[]) {
  if (argc < 3)
    return 1;
  char payload[128], message[128];
  printf_sn(payload, 128, "SET_CONFIG POLLING_INTERVAL %s", argv[2]);
  printf_sn(message, 128, "Polling interval set to %s ms.", argv[2]);
  return config_write(argc, argv, payload, "POLLING_INTERVAL", argv[2],
                      message);
}

int cli_config_threshold(int argc, char *argv[]) {
  if (argc < 3)
    return 1;
  char payload[128], message[128];
  printf_sn(payload, 128, "SET_CONFIG LOAD_THRESHOLD %s", argv[2]);
  printf_sn(message, 128, "Compute load threshold set to %s%%.", argv[2]);
  return config_write(argc, argv, payload, "LOAD_THRESHOLD", argv[2], message);
}

int cli_config_fallback(int argc, char *argv[]) {
  if (argc < 3)
    return 1;
  char payload[128], message[128];
  printf_sn(payload, 128, "SET_CONFIG FALLBACK_PROFILE %s", argv[2]);
  printf_sn(message, 128, "Fallback baseline profile set to '%s'.", argv[2]);
  return config_write(argc, argv, payload, "FALLBACK_PROFILE", argv[2],
                      message);
}

int cli_config_detection(int argc, char *argv[]) {
  if (argc < 3)
    return 1;
  char payload[128], message[128];
  printf_sn(payload, 128, "SET_CONFIG DETECTION_LEVEL %s", argv[2]);
  printf_sn(message, 128, "Detection mode shifted to '%s'.", argv[2]);
  return config_write(argc, argv, payload, "DETECTION_LEVEL", argv[2], message);
}

int cli_config_polling(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  config_write(argc, argv, "SET_CONFIG DETECTION_LEVEL 0", "DETECTION_LEVEL",
               "0", "Detection mode shifted to Polling Heuristics.");
  return config_write(argc, argv, "SET_CONFIG EBPF_ENABLE 0", "EBPF_ENABLE",
                      "0", "  ... eBPF tracker unhooked from kernel.");
}

int cli_config_ebpf(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  config_write(argc, argv, "SET_CONFIG DETECTION_LEVEL 1", "DETECTION_LEVEL",
               "1", "Detection mode shifted to eBPF Zero-latency.");
  return config_write(
      argc, argv, "SET_CONFIG EBPF_ENABLE 1", "EBPF_ENABLE", "1",
      "  ... eBPF tracker successfully attached to kernel events.");
}

int cli_config_server(int argc, char *argv[]) {
  if (argc < 3)
    return 1;
  char payload[256], message[256], value[128];
  if (argc >= 4) {
    printf_sn(value, 128, "%s:%s", argv[2], argv[3]);
    printf_sn(payload, 256, "SET_CONFIG SERVER_ADDRESS %s:%s", argv[2],
              argv[3]);
  } else {
    printf_sn(value, 128, "%s", argv[2]);
    printf_sn(payload, 256, "SET_CONFIG SERVER_ADDRESS %s", argv[2]);
  }
  printf_sn(message, 256, "Dashboard server set to %s.", value);
  return config_write(argc, argv, payload, "SERVER_ADDRESS", value, message);
}

int cli_config_add(int argc, char *argv[]) {
  if (argc < 3) {
    return 1;
  }
  char payload[256], message[256];
  printf_sn(payload, 256, "GAME_ADD %s", argv[2]);
  printf_sn(message, 256, "Process '%s' appended to trigger list.", argv[2]);
  return config_write(argc, argv, payload, NULL, NULL, message);
}

int cli_config_remove(int argc, char *argv[]) {
  if (argc < 3) {
    return 1;
  }
  char payload[256], message[256];
  printf_sn(payload, 256, "GAME_REMOVE %s", argv[2]);
  printf_sn(message, 256, "Process '%s' stripped from trigger list.", argv[2]);
  return config_write(argc, argv, payload, NULL, NULL, message);
}

int cli_config_list(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  char response[2048] = {0};

  if (socket_send("GAMES_LIST", response, sizeof(response)) == ERR_SUCCESS) {
    printf_string("--- Active Daemon Trigger List ---");
    printf_string("%s", response);
    return ERR_SUCCESS;
  }

  int fd = open(CONFIG_PATH, O_RDONLY);
  if (fd < 0) {
    journal_error(ERR_LOST, CONFIG_PATH);
    return ERR_LOST;
  }

  printf_string("--- Saved Trigger List (Daemon Offline) ---");
  char buf[4096];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    char *line = buf;
    char *next;
    while (line && *line) {
      next = strchr(line, '\n');
      if (next)
        *next = '\0';
      printf_string(" - %s", line);
      if (next)
        line = next + 1;
      else
        break;
    }
  }
  close(fd);
  return ERR_SUCCESS;
}

int cli_config_profile(int argc, char *argv[]) {
  if (argc < 4) {
    journal_error(
        ERR_PROFILE,
        "Usage: X3D Toggle profile [add|save|load|delete] [profile_name]");
    return ERR_SYNTAX;
  }

  char *action = argv[2], *profile_name = argv[3];
  char payload[256], msg[256];

  printf_sn(payload, sizeof(payload), "PROFILE %s %s", action, profile_name);

  if (strcmp(action, "add") == 0 || strcmp(action, "save") == 0) {
    printf_sn(msg, sizeof(msg),
              "Snapshot of active topology and limits saved to profile '%s'.",
              profile_name);
  } else if (strcmp(action, "load") == 0) {
    printf_sn(msg, sizeof(msg),
              "Profile '%s' loaded and hardware states applied.", profile_name);
  } else if (strcmp(action, "delete") == 0) {
    printf_sn(msg, sizeof(msg), "Profile '%s' deleted from persistent storage.",
              profile_name);
  } else {
    journal_error(ERR_SYNTAX, action);
    return ERR_SYNTAX;
  }

  int res = socket_send(payload, NULL, 0);
  if (res == ERR_SUCCESS) {
    journal_info(PROFILE_ACTION, msg);
    return ERR_SUCCESS;
  }

  journal_error(ERR_IPC, -1);
  return ERR_IPC;
}

int cli_config_sync(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  if (socket_send("CONFIG_SYNC", NULL, 0) == ERR_SUCCESS) {
    journal_info(GAME_SYNCED, "Runtime configuration flushed to disk.");
    return ERR_SUCCESS;
  }
  journal_error(ERR_IPC, -1);
  return ERR_IPC;
}

int cli_config_generate(int argc, char *argv[]) {
  const char *settings_src = (argc > 2) ? argv[2] : "config/settings.conf";
  const char *games_src = (argc > 3) ? argv[3] : "config/games.conf";
  const char *dest = (argc > 4) ? argv[4] : CONFIG_PATH;

  printf_string("🛠️ Generating X3D Toggle Configuration based on defaults...");
  int res = config_generate(settings_src, games_src, dest);
  if (res == ERR_SUCCESS) {
    printf_string("✅ Configuration successfully assembled at %s", dest);
  } else {
    journal_error(res, dest);
  }
  return res;
}

void config_load(DaemonConfig *cfg) {
  cfg->polling_interval = CONFIG_POLLING_INTERVAL;
  cfg->refresh_interval = CONFIG_REFRESH_INTERVAL;
  cfg->dev_enable = CONFIG_DEV_ENABLE;
  cfg->affinity_level = CONFIG_AFFINITY_LEVEL;
  scat(cfg->affinity_mask, CONFIG_AFFINITY_MASK, 63);

  cfg->load_threshold = CONFIG_LOAD_THRESHOLD;
  cfg->detection_level = CONFIG_DETECTION_LEVEL;
  cfg->ebpf_enable = CONFIG_EBPF_ENABLE;
  cfg->debug_enable = CONFIG_DEBUG_ENABLE;
  scat(cfg->daemon_state, "default", 31);
  scat(cfg->fallback_profile, CONFIG_FALLBACK_PROFILE, 63);
  scat(cfg->server_address, CONFIG_SERVER_ADDRESS, 127);

  int fd = open(DAEMON_CONF_PATH, O_RDONLY);
  if (fd < 0) {
  }

  if (fd >= 0) {
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = '\0';
      char *ln = buf;
      char *nxt;
      while (ln && *ln) {
        nxt = strchr(ln, '\n');
        if (nxt) *nxt = '\0';
        if (ln[0] != '#' && ln[0] != '\0') {
          char *val = strchr(ln, '=');
          if (val) {
            *val = '\0';
            val++;
            if (strcmp(ln, "POLLING_INTERVAL") == 0) cfg->polling_interval = atoi(val);
            else if (strcmp(ln, "REFRESH_INTERVAL") == 0) cfg->refresh_interval = atof(val);
            else if (strcmp(ln, "DEV_ENABLE") == 0) cfg->dev_enable = atoi(val);
            else if (strcmp(ln, "LOAD_THRESHOLD") == 0) cfg->load_threshold = atof(val);
            else if (strcmp(ln, "DETECTION_LEVEL") == 0) cfg->detection_level = atoi(val);
            else if (strcmp(ln, "EBPF_ENABLE") == 0) cfg->ebpf_enable = atoi(val);
            else if (strcmp(ln, "DEBUG_ENABLE") == 0) cfg->debug_enable = atoi(val);
            else if (strcmp(ln, "DAEMON_STATE") == 0) scat(cfg->daemon_state, val, 31);
            else if (strcmp(ln, "FALLBACK_PROFILE") == 0) scat(cfg->fallback_profile, val, 63);
            else if (strcmp(ln, "SERVER_ADDRESS") == 0) scat(cfg->server_address, val, 127);
          }
        }
        ln = nxt ? nxt + 1 : (void*)0;
      }
    }
    close(fd);
  }
}

void config_update(const char *key, const char *val) {
  int fd = open(DAEMON_CONF_PATH, O_RDONLY);
  if (fd < 0) return;

  char buf[8192];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) return;
  buf[n] = '\0';

  char out[8192] = "";
  char *line = buf;
  char *next;
  int found = 0;
  while (line && *line) {
    next = strchr(line, '\n');
    if (next) *next = '\0';

    if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == '=') {
      strlcat_local(out, key, 8192);
      strlcat_local(out, "=", 8192);
      strlcat_local(out, val, 8192);
      strlcat_local(out, "\n", 8192);
      found = 1;
    } else {
      strlcat_local(out, line, 8192);
      strlcat_local(out, "\n", 8192);
    }
    line = next ? next + 1 : (void*)0;
  }

  if (!found) {
    strlcat_local(out, key, 8192);
    strlcat_local(out, "=", 8192);
    strlcat_local(out, val, 8192);
    strlcat_local(out, "\n", 8192);
  }

  int out_fd = open(DAEMON_CONF_PATH, O_WRONLY | O_TRUNC);
  if (out_fd >= 0) {
    write(out_fd, out, strlen(out));
    close(out_fd);
  }
}

int config_generate(const char *src, const char *games, const char *dest) {
  (void)games; // Processed by independent logic
  int sfd = open(src, O_RDONLY);
  if (sfd < 0) return ERR_IO;
  
  char buf[4096];
  ssize_t n = read(sfd, buf, sizeof(buf)-1);
  close(sfd);
  if (n <= 0) return ERR_IO;
  buf[n] = '\0';

  int dfd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0664);
  if (dfd < 0) return ERR_IO;
  write(dfd, buf, (size_t)n);
  close(dfd);
  return ERR_SUCCESS;
}

int cli_config_update(int argc, char *argv[]) {
  (void)argc; (void)argv;
  printf_string("🛠️  Regenerating system configuration from source templates...");

  char cmd_buf[512];
  printf_sn(cmd_buf, sizeof(cmd_buf), "X3D_EXEC=1 sh %s/scripts/framework/config.sh --update", USR_LIBS);
  
  int res = system(cmd_buf);
  if (res != 0) {
    journal_error(ERR_IO, "config.sh --update");
    return ERR_IO;
  }

  if (socket_send("CONFIG_SYNC", NULL, 0) == ERR_SUCCESS)
    printf_string("✅ Configuration rebuilt and daemon synced.");
  else
    printf_string("✅ Configuration rebuilt (daemon offline — will apply on next start).");
  return ERR_SUCCESS;
}

/* end of CONFIG.C */
