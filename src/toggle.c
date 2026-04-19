/* CLI Entry Point for the X3D Toggle Project
 *
 * `x3d-toggle.c`
 *
 * Frontend layer for passing CLI to the daemon.
 * Model-agnostic implementation for heterogeneous architectures.
 */

#include "xui.h"
#include "cli.h"
#include "error.h"
#include "libc.h"

static void printf_help(void) {
  printf_br();
  printf_center("X3D Toggle - Advanced CLI (IPC Enabled)");
  printf_center("Usage: x3d-toggle [COMMAND|MODE] [ARGS...]");
  printf_center("   or: x3d -[COMMAND|MODE] [ARGS...]\\n");
  printf_br();

  for (int i = 0; cmd_table[i].name != NULL; i++) {
    if (strstr(cmd_table[i].help, "Alias for"))
      continue;
    if (i == 0) {
      printf_string("Daemon Control:");
    }

    if (i == 5) {
      printf_string("Topology & Thread Management:");
    }

    if (i == 12) {
      printf_string("Configuration & Rulesets:");
    }

    if (i == 22) {
      printf_string("Diagnostics:");
    }
    
    char alias_buf[16] = "";
    for (int j = 0; cmd_table[j].name != NULL; j++) {
      if (cmd_table[i].handler == cmd_table[j].handler && i != j) {
        scat(alias_buf, cmd_table[j].name, sizeof(alias_buf));
        break;
      }
    }

    printf_string("  %-12s  %-12s  %s", cmd_table[i].name, alias_buf, cmd_table[i].help);
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
    journal_error(ERR_USAGE);
    return ERR_USAGE;
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
