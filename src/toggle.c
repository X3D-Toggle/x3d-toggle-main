/* CLI Entry Point for the X3D Toggle Project
 * `toggle.c`
 * Frontend layer for passing CLI to the daemon.
 * Model-agnostic implementation for heterogeneous architectures.
 */

#include "cli.h"
#include "error.h"
#include "libc.h"
#include "xui.h"

#define ALIAS_BUF_SIZE 64

static void printf_section(const char *title, int first) {
  if (!first)
    printf_br();
  printf_string("%s", title);
}

static void printf_help(void) {
  printf_br();
  printf_center("X3D Toggle - Advanced CLI (IPC Enabled)");
  printf_center("Usage: x3d-toggle                   (Launch Desktop GUI)");
  printf_center("   or: x3d-toggle [COMMAND] [ARGS...]");
  printf_center("   or: x3d -[COMMAND|MODE] [ARGS...]");
  printf_br();

  for (int i = 0; cmd_table[i].name != NULL; i++) {
    if (strstr(cmd_table[i].help, "Alias for") ||
        strncmp(cmd_table[i].help, "Hidden:", 7) == 0 ||
        strncmp(cmd_table[i].help, "Internal:", 9) == 0)
      continue;

    if (strcmp(cmd_table[i].name, "enable") == 0) {
      printf_section("Daemon Control:", 1);
    } else if (strcmp(cmd_table[i].name, "cache") == 0) {
      printf_section("Topology & Modes:", 0);
    } else if (strcmp(cmd_table[i].name, "interval") == 0) {
      printf_section("Configuration & Rulesets:", 0);
    } else if (strcmp(cmd_table[i].name, "dialog") == 0) {
      printf_section("Monitoring & Diagnostics:", 0);
    } else if (strcmp(cmd_table[i].name, "perf") == 0) {
      printf_section("CPPC & Power Scaling:", 0);
    } else if (strcmp(cmd_table[i].name, "sched-gaming") == 0) {
      printf_section("Scheduler & IRQ Affinity:", 0);
    } else if (strcmp(cmd_table[i].name, "advanced") == 0) {
      printf_section("System & Desktop:", 0);
    }

    char alias_buf[ALIAS_BUF_SIZE] = "";
    size_t alias_len = 0;
    for (int j = 0; cmd_table[j].name != NULL; j++) {
      if (i != j && cmd_table[i].handler == cmd_table[j].handler &&
          strstr(cmd_table[j].help, "Alias for") != NULL) {
        int written =
            printf_sn(alias_buf + alias_len, sizeof(alias_buf) - alias_len,
                      "%s%s", alias_len > 0 ? ", " : "", cmd_table[j].name);
        if (written > 0 && alias_len + (size_t)written < sizeof(alias_buf))
          alias_len += (size_t)written;
      }
    }

    printf_string("  %-16s  %-12s  %s", cmd_table[i].name, alias_buf,
                  cmd_table[i].help);
  }

  printf_br();
  printf_string("Options:");
  printf_string(
      "  help, h, manual, book, instructions    Show this help message");
  printf_string(
      "  -h, --h, -help, --help                 Show this help message");
  printf_br();
}

int daemon(int argc, char *argv[]) {
  if (argc < 2) {
    return cli_gui_launch(argc, argv);
  }

  if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 ||
      strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "help") == 0 ||
      strcmp(argv[1], "h") == 0 || strcmp(argv[1], "--h") == 0 ||
      strcmp(argv[1], "manual") == 0 || strcmp(argv[1], "book") == 0 ||
      strcmp(argv[1], "instructions") == 0) {
    printf_help();
    return 0;
  }

  if (strcmp(argv[1], "--gen-config") == 0) {
    return cli_config_generate(argc, argv);
  }

  const Command *cmd = find_command(argv[1]);
  if (cmd) {
    return cmd->handler(argc, argv);
  }

  extern int cli_misc_fallback(int argc, char *argv[]);
  return cli_misc_fallback(argc, argv);
}

/*/ end of X3D_TOGGLE.C /*/
