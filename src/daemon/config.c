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

#include "../build/config.h" 
#include "xui.h"
#include "cli.h"
#include "error.h"
#include "games.h"
#include "ipc.h"
#include "libc.h"

void config_probe_capabilities(DaemonConfig *cfg);

static int config_write(int argc, char *argv[], const char *ipc_command,
                        const char *config_key, const char *config_value,
                        const char *user_message) {
  (void)argc; (void)argv;
  int ipc_res = socket_send(ipc_command, NULL, 0);
  if (ipc_res == 0) {
    printf_string("✅ %s (Active daemon synced)", user_message);
    return 0;
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
  char payload[256], message[256];
  char ip[128] = "";
  char port[32] = "";
  
  const char *colon = strchr(argv[2], ':');
  if (colon) {
    size_t ip_len = colon - argv[2];
    if (ip_len >= sizeof(ip)) ip_len = sizeof(ip) - 1;
    memcpy(ip, argv[2], ip_len);
    ip[ip_len] = '\0';
    strncpy(port, colon + 1, sizeof(port) - 1);
  } else {
    strncpy(ip, argv[2], sizeof(ip) - 1);
    if (argc >= 4) {
      strncpy(port, argv[3], sizeof(port) - 1);
    }
  }

  printf_sn(payload, sizeof(payload), "SET_CONFIG SERVER_ADDRESS %s", ip);
  printf_sn(message, sizeof(message), "Dashboard server IP set to %s.", ip);
  config_write(argc, argv, payload, "SERVER_ADDRESS", ip, message);

  if (port[0]) {
    printf_sn(payload, sizeof(payload), "SET_CONFIG SERVER_PORT %s", port);
    printf_sn(message, sizeof(message), "Dashboard server port set to %s.", port);
    config_write(argc, argv, payload, "SERVER_PORT", port, message);
  }
  
  return 0;
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
  config_probe_capabilities(cfg);
  cfg->polling_interval = CONFIG_POLLING_INTERVAL;
  cfg->refresh_interval = CONFIG_REFRESH_INTERVAL;
  cfg->dev_enable = CONFIG_DEV_ENABLE;
  cfg->affinity_level = CONFIG_AFFINITY_LEVEL;
  printf_sn(cfg->affinity_mask, 63, "%s", CONFIG_AFFINITY_MASK);
  printf_sn(cfg->affinity_freq_mask, 63, "%s", CONFIG_AFFINITY_FREQ_MASK);

  cfg->load_threshold = CONFIG_LOAD_THRESHOLD;
  cfg->detection_level = CONFIG_DETECTION_LEVEL;
  cfg->ebpf_enable = CONFIG_EBPF_ENABLE;
  cfg->debug_enable = CONFIG_DEBUG_ENABLE;
  printf_sn(cfg->daemon_state, 31, "default");
  printf_sn(cfg->fallback_profile, 63, "%s", CONFIG_FALLBACK_PROFILE);
  cfg->server_enabled = CONFIG_SERVER_ENABLED;
  printf_sn(cfg->server_address, 127, "%s", CONFIG_SERVER_ADDRESS);
  cfg->server_port = CONFIG_SERVER_PORT;
  printf_sn(cfg->server_ssh, 31, "%s", CONFIG_SERVER_SSH);
  printf_sn(cfg->lint_clang_diagnostic, 15, "enabled");
  printf_sn(cfg->lint_clang_bugprone, 15, "enabled");
  printf_sn(cfg->lint_clang_modernize, 15, "enabled");
  printf_sn(cfg->lint_clang_readability, 15, "enabled");
  printf_sn(cfg->lint_clang_performance, 15, "enabled");
  printf_sn(cfg->lint_clang_portability, 15, "enabled");
  printf_sn(cfg->lint_clang_analyzer, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_all, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_warning, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_style, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_performance, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_portability, 15, "enabled");
  printf_sn(cfg->lint_cppcheck_information, 15, "disabled");
  printf_sn(cfg->lint_cppcheck_unused, 15, "enabled");
  printf_sn(cfg->lint_valgrind_mode, 15, "full");
  printf_sn(cfg->lint_valgrind_kinds, 31, "all");
  printf_sn(cfg->lint_valgrind_origins, 15, "enabled");

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
            else if (strcmp(ln, "DAEMON_STATE") == 0) printf_sn(cfg->daemon_state, 31, "%s", val);
            else if (strcmp(ln, "FALLBACK_PROFILE") == 0) printf_sn(cfg->fallback_profile, 63, "%s", val);
            else if (strcmp(ln, "SERVER_ENABLED") == 0) cfg->server_enabled = (strcmp(val, "true") == 0 || atoi(val) != 0);
            else if (strcmp(ln, "SERVER_ADDRESS") == 0) printf_sn(cfg->server_address, 127, "%s", val);
            else if (strcmp(ln, "SERVER_PORT") == 0) cfg->server_port = atoi(val);
            else if (strcmp(ln, "SERVER_SSH") == 0) printf_sn(cfg->server_ssh, 31, "%s", val);
            else if (strcmp(ln, "AFFINITY_MASK") == 0) printf_sn(cfg->affinity_mask, 63, "%s", val);
            else if (strcmp(ln, "AFFINITY_FREQ_MASK") == 0) printf_sn(cfg->affinity_freq_mask, 63, "%s", val);
            /* Advanced */
            else if (strcmp(ln, "SCHED_BORE") == 0) cfg->sched_bore = atoi(val);
            else if (strcmp(ln, "SCHED_BIT_SHIFT") == 0) cfg->sched_bit_shift = atoi(val);
            else if (strcmp(ln, "SCHED_BURST_FORK") == 0) cfg->sched_burst_fork = atoi(val);
            else if (strcmp(ln, "SCHED_SLICE_US") == 0) cfg->sched_slice_us = atoi(val);
            else if (strcmp(ln, "VM_MAX_MAP") == 0) cfg->vm_max_map = atoi(val);
            else if (strcmp(ln, "SPLIT_LOCK_DETECT") == 0) cfg->split_lock_detect = atoi(val);
            else if (strcmp(ln, "NMI_WATCHDOG") == 0) cfg->nmi_watchdog = atoi(val);
            else if (strcmp(ln, "THP_MODE") == 0) printf_sn(cfg->thp_mode, 15, "%s", val);
            /* Networking */
            else if (strcmp(ln, "NET_QDISC") == 0) printf_sn(cfg->net_qdisc, 31, "%s", val);
            else if (strcmp(ln, "NET_FASTOPEN") == 0) cfg->net_fastopen = atoi(val);
            else if (strcmp(ln, "NET_RP_FILTER") == 0) cfg->net_rp_filter = atoi(val);
            else if (strcmp(ln, "NET_SOURCE_ROUTE") == 0) cfg->net_source_route = atoi(val);
            /* IRQ Subsystem */
            else if (strcmp(ln, "IRQ_ENABLE") == 0) cfg->irq_enable = atoi(val);
            else if (strcmp(ln, "IRQ_GPU") == 0) cfg->irq_gpu = atoi(val);
            else if (strcmp(ln, "IRQ_NVME") == 0) cfg->irq_nvme = atoi(val);
            else if (strcmp(ln, "IRQ_USB") == 0) cfg->irq_usb = atoi(val);
            else if (strcmp(ln, "IRQ_NIC") == 0) cfg->irq_nic = atoi(val);
            else if (strcmp(ln, "IRQ_AUDIO") == 0) cfg->irq_audio = atoi(val);
            else if (strcmp(ln, "IRQ_COALESCE") == 0) cfg->irq_coalesce = atoi(val);
            else if (strcmp(ln, "IRQ_WATCH") == 0) cfg->irq_watch = atoi(val);
            /* Developer & Linting */
            else if (strcmp(ln, "ADVANCED_CONFIG_ENABLE") == 0) cfg->advanced_config_enable = atoi(val);
            else if (strcmp(ln, "FORCE_UNSUPPORTED_DISPLAY") == 0) cfg->force_unsupported_display = atoi(val);
            else if (strcmp(ln, "LINT_CLANG_DIAGNOSTIC") == 0) printf_sn(cfg->lint_clang_diagnostic, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_BUGPRONE") == 0) printf_sn(cfg->lint_clang_bugprone, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_MODERNIZE") == 0) printf_sn(cfg->lint_clang_modernize, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_READABILITY") == 0) printf_sn(cfg->lint_clang_readability, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_PERFORMANCE") == 0) printf_sn(cfg->lint_clang_performance, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_PORTABILITY") == 0) printf_sn(cfg->lint_clang_portability, 15, "%s", val);
            else if (strcmp(ln, "LINT_CLANG_ANALYZER") == 0) printf_sn(cfg->lint_clang_analyzer, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_ALL") == 0) printf_sn(cfg->lint_cppcheck_all, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_WARNING") == 0) printf_sn(cfg->lint_cppcheck_warning, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_STYLE") == 0) printf_sn(cfg->lint_cppcheck_style, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_PERFORMANCE") == 0) printf_sn(cfg->lint_cppcheck_performance, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_PORTABILITY") == 0) printf_sn(cfg->lint_cppcheck_portability, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_INFORMATION") == 0) printf_sn(cfg->lint_cppcheck_information, 15, "%s", val);
            else if (strcmp(ln, "LINT_CPPCHECK_UNUSED") == 0) printf_sn(cfg->lint_cppcheck_unused, 15, "%s", val);
            else if (strcmp(ln, "LINT_VALGRIND_MODE") == 0) printf_sn(cfg->lint_valgrind_mode, 15, "%s", val);
            else if (strcmp(ln, "LINT_VALGRIND_KINDS") == 0) printf_sn(cfg->lint_valgrind_kinds, 31, "%s", val);
            else if (strcmp(ln, "LINT_VALGRIND_ORIGINS") == 0) printf_sn(cfg->lint_valgrind_origins, 15, "%s", val);
            else if (strcmp(ln, "JOURNAL_KEEP") == 0) cfg->journal_keep = atoi(val);
            else if (strcmp(ln, "JOURNAL_MAX_MB") == 0) cfg->journal_max_mb = atoi(val);
          }
        }
        ln = nxt ? nxt + 1 : (void*)0;
      }
    }
    close(fd);
  }
}

const char *config_get(const char *key) {
  static char val[128];
  extern DaemonConfig cfg;
  if (strcmp(key, "POLLING_INTERVAL") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.polling_interval);
  else if (strcmp(key, "LOAD_THRESHOLD") == 0)
    printf_sn(val, sizeof(val), "%d", (int)cfg.load_threshold);
  else if (strcmp(key, "DETECTION_LEVEL") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.detection_level);
  else if (strcmp(key, "EBPF_ENABLE") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.ebpf_enable);
  else if (strcmp(key, "DEBUG_ENABLE") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.debug_enable);
  else if (strcmp(key, "DEV_ENABLE") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.dev_enable);
  else if (strcmp(key, "DAEMON_STATE") == 0)
    return cfg.daemon_state;
  else if (strcmp(key, "FALLBACK_PROFILE") == 0)
    return cfg.fallback_profile;
  else if (strcmp(key, "SERVER_ENABLED") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.server_enabled);
  else if (strcmp(key, "SERVER_ADDRESS") == 0)
    return cfg.server_address;
  else if (strcmp(key, "SERVER_PORT") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.server_port);
  else if (strcmp(key, "SERVER_SSH") == 0)
    return cfg.server_ssh;
  else if (strcmp(key, "AFFINITY_MASK") == 0)
    return cfg.affinity_mask;
  else if (strcmp(key, "AFFINITY_FREQ_MASK") == 0)
    return cfg.affinity_freq_mask;
  else if (strcmp(key, "AFFINITY_LEVEL") == 0)
    printf_sn(val, sizeof(val), "%d", cfg.affinity_level);
  /* Advanced */
  else if (strcmp(key, "SCHED_BORE") == 0) printf_sn(val, sizeof(val), "%d", cfg.sched_bore);
  else if (strcmp(key, "SCHED_BIT_SHIFT") == 0) printf_sn(val, sizeof(val), "%d", cfg.sched_bit_shift);
  else if (strcmp(key, "SCHED_BURST_FORK") == 0) printf_sn(val, sizeof(val), "%d", cfg.sched_burst_fork);
  else if (strcmp(key, "SCHED_SLICE_US") == 0) printf_sn(val, sizeof(val), "%d", cfg.sched_slice_us);
  else if (strcmp(key, "VM_MAX_MAP") == 0) printf_sn(val, sizeof(val), "%d", cfg.vm_max_map);
  else if (strcmp(key, "SPLIT_LOCK_DETECT") == 0) printf_sn(val, sizeof(val), "%d", cfg.split_lock_detect);
  else if (strcmp(key, "NMI_WATCHDOG") == 0) printf_sn(val, sizeof(val), "%d", cfg.nmi_watchdog);
  else if (strcmp(key, "THP_MODE") == 0) return cfg.thp_mode;
  /* Networking */
  else if (strcmp(key, "NET_QDISC") == 0) return cfg.net_qdisc;
  else if (strcmp(key, "NET_FASTOPEN") == 0) printf_sn(val, sizeof(val), "%d", cfg.net_fastopen);
  else if (strcmp(key, "NET_RP_FILTER") == 0) printf_sn(val, sizeof(val), "%d", cfg.net_rp_filter);
  else if (strcmp(key, "NET_SOURCE_ROUTE") == 0) printf_sn(val, sizeof(val), "%d", cfg.net_source_route);
  /* IRQ Subsystem */
  else if (strcmp(key, "IRQ_ENABLE") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_enable);
  else if (strcmp(key, "IRQ_GPU") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_gpu);
  else if (strcmp(key, "IRQ_NVME") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_nvme);
  else if (strcmp(key, "IRQ_USB") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_usb);
  else if (strcmp(key, "IRQ_NIC") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_nic);
  else if (strcmp(key, "IRQ_AUDIO") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_audio);
  else if (strcmp(key, "IRQ_COALESCE") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_coalesce);
  else if (strcmp(key, "IRQ_WATCH") == 0) printf_sn(val, sizeof(val), "%d", cfg.irq_watch);
  /* Developer & Linting */
  else if (strcmp(key, "ADVANCED_CONFIG_ENABLE") == 0) printf_sn(val, sizeof(val), "%d", cfg.advanced_config_enable);
  else if (strcmp(key, "FORCE_UNSUPPORTED_DISPLAY") == 0) printf_sn(val, sizeof(val), "%d", cfg.force_unsupported_display);
  else if (strcmp(key, "LINT_CLANG_DIAGNOSTIC") == 0) return cfg.lint_clang_diagnostic;
  else if (strcmp(key, "LINT_CLANG_BUGPRONE") == 0) return cfg.lint_clang_bugprone;
  else if (strcmp(key, "LINT_CLANG_MODERNIZE") == 0) return cfg.lint_clang_modernize;
  else if (strcmp(key, "LINT_CLANG_READABILITY") == 0) return cfg.lint_clang_readability;
  else if (strcmp(key, "LINT_CLANG_PERFORMANCE") == 0) return cfg.lint_clang_performance;
  else if (strcmp(key, "LINT_CLANG_PORTABILITY") == 0) return cfg.lint_clang_portability;
  else if (strcmp(key, "LINT_CLANG_ANALYZER") == 0) return cfg.lint_clang_analyzer;
  else if (strcmp(key, "LINT_CPPCHECK_ALL") == 0) return cfg.lint_cppcheck_all;
  else if (strcmp(key, "LINT_CPPCHECK_WARNING") == 0) return cfg.lint_cppcheck_warning;
  else if (strcmp(key, "LINT_CPPCHECK_STYLE") == 0) return cfg.lint_cppcheck_style;
  else if (strcmp(key, "LINT_CPPCHECK_PERFORMANCE") == 0) return cfg.lint_cppcheck_performance;
  else if (strcmp(key, "LINT_CPPCHECK_PORTABILITY") == 0) return cfg.lint_cppcheck_portability;
  else if (strcmp(key, "LINT_CPPCHECK_INFORMATION") == 0) return cfg.lint_cppcheck_information;
  else if (strcmp(key, "LINT_CPPCHECK_UNUSED") == 0) return cfg.lint_cppcheck_unused;
  else if (strcmp(key, "LINT_VALGRIND_MODE") == 0) return cfg.lint_valgrind_mode;
  else if (strcmp(key, "LINT_VALGRIND_KINDS") == 0) return cfg.lint_valgrind_kinds;
  else if (strcmp(key, "LINT_VALGRIND_ORIGINS") == 0) return cfg.lint_valgrind_origins;
  else if (strcmp(key, "JOURNAL_KEEP") == 0) printf_sn(val, sizeof(val), "%d", cfg.journal_keep);
  else if (strcmp(key, "JOURNAL_MAX_MB") == 0) printf_sn(val, sizeof(val), "%d", cfg.journal_max_mb);
  else if (strcmp(key, "REFRESH_INTERVAL") == 0)
    printf_sn(val, sizeof(val), "%.2f", cfg.refresh_interval);
  else
    return "ERR";
  return val;
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
  (void)games;
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
  
  int irq_fd = open("config/irq.conf", O_RDONLY);
  if (irq_fd >= 0) {
    ssize_t in = read(irq_fd, buf, sizeof(buf)-1);
    if (in > 0) {
      write(dfd, "\n", 1);
      write(dfd, buf, (size_t)in);
    }
    close(irq_fd);
  }
  
  close(dfd);
  return ERR_SUCCESS;
}

int cli_config_update(int argc, char *argv[]) {
  (void)argc; (void)argv;
  printf_string("🛠️  Regenerating system configuration from source templates...");

  char config_path[256];
  printf_sn(config_path, sizeof(config_path), "%s/scripts/framework/config.sh", USR_LIBS);

  pid_t pid = fork();
  if (pid == 0) {
    char *args[] = {(char *)"/bin/sh", config_path, (char *)"--update", NULL};
    char *envp[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, envp);
    _exit(1);
  }

  int res = ERR_IO;
  if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    res = (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
  }

  if (res != ERR_SUCCESS) {
    journal_error(ERR_IO, "config.sh --update");
    return ERR_IO;
  }

  if (socket_send("CONFIG_SYNC", NULL, 0) == ERR_SUCCESS)
    printf_string("✅ Configuration rebuilt and daemon synced.");
  else
    printf_string("✅ Configuration rebuilt (daemon offline — will apply on next start).");
  return ERR_SUCCESS;
}

void config_probe_capabilities(DaemonConfig *cfg) {
    cfg->capabilities = 0;
    if (access("/proc/sys/kernel/sched_bore", F_OK) == 0) cfg->capabilities |= CAP_BORE;
    if (access("/sys/kernel/mm/transparent_hugepage/enabled", F_OK) == 0) cfg->capabilities |= CAP_THP;
    if (access("/proc/sys/kernel/split_lock_mitigate", F_OK) == 0) cfg->capabilities |= CAP_SPLIT_LOCK;
    if (access("/proc/sys/kernel/nmi_watchdog", F_OK) == 0) cfg->capabilities |= CAP_NMI_WATCHDOG;
    
    /* Network checks - simplified for now */
    if (access("/proc/sys/net/ipv4/tcp_fastopen", F_OK) == 0) cfg->capabilities |= CAP_FASTOPEN;
    if (access("/proc/sys/net/core/default_qdisc", F_OK) == 0) cfg->capabilities |= CAP_QDISC;
}

int cli_advanced_config(int argc, char *argv[]) {
    if (argc < 3) return 1;
    char payload[256], message[256];
    
    if (strcmp(argv[2], "get") == 0) {
        if (argc < 4) return 1;
        printf_sn(payload, 256, "ADVANCED_GET %s", argv[3]);
        char resp[256] = {0};
        if (socket_send(payload, resp, sizeof(resp)) == 0) {
            printf_string("%s = %s", argv[3], resp);
            return 0;
        }
        journal_error(ERR_IPC, -1);
        return 1;
    } else if (strcmp(argv[2], "set") == 0) {
        if (argc < 5) return 1;
        printf_sn(payload, 256, "ADVANCED_SET %s %s", argv[3], argv[4]);
        printf_sn(message, 256, "Advanced configuration '%s' set to '%s'.", argv[3], argv[4]);
        return config_write(argc, argv, payload, argv[3], argv[4], message);
    }
    return 1;
}

/* end of CONFIG.C */
