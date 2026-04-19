/* Shared dispatch table for the CLI backend for the X3D Toggle project
 *
 * `cli.h` -Header only
 */

#ifndef CLI_H
#define CLI_H

#include "cppc.h" // IWYU pragma: keep

typedef int (*cmd_handler)(int argc, char *argv[]);

typedef struct {
  const char *name;
  cmd_handler handler;
  const char *help;
} Command;

extern const Command cmd_table[];

const Command *find_command(const char *name);

int cli_daemon_start(int argc, char *argv[]);
int cli_daemon_enable(int argc, char *argv[]);
int cli_daemon_wake(int argc, char *argv[]);
int cli_daemon_sleep(int argc, char *argv[]);
int cli_daemon_stop(int argc, char *argv[]);
int cli_daemon_disable(int argc, char *argv[]);
int cli_mode_cache(int argc, char *argv[]);
int cli_mode_frequency(int argc, char *argv[]);
int cli_mode_dual(int argc, char *argv[]);
int cli_mode_swap(int argc, char *argv[]);
int cli_mode_auto(int argc, char *argv[]);
int cli_mode_default(int argc, char *argv[]);
int cli_mode_reset(int argc, char *argv[]);
int cli_config_sync(int argc, char *argv[]);
int cli_config_update(int argc, char *argv[]);
int cli_config_interval(int argc, char *argv[]);
int cli_config_threshold(int argc, char *argv[]);
int cli_config_fallback(int argc, char *argv[]);
int cli_config_detection(int argc, char *argv[]);
int cli_config_polling(int argc, char *argv[]);
int cli_config_ebpf(int argc, char *argv[]);
int cli_config_server(int argc, char *argv[]);
int cli_config_add(int argc, char *argv[]);
int cli_config_remove(int argc, char *argv[]);
int cli_config_list(int argc, char *argv[]);
int cli_config_profile(int argc, char *argv[]);
int cli_config_generate(int argc, char *argv[]);
int cli_framework(int argc, char *argv[]);
int cli_status_dialog(int argc, char *argv[]);
int cli_status_monitor(int argc, char *argv[]);
int cli_status_owl(int argc, char *argv[]);
int cli_diag_linter(int argc, char *argv[]);
int cli_diag_debug(int argc, char *argv[]);
int cli_diag_status(int argc, char *argv[]);
int cli_diag_journal(int argc, char *argv[]);
int cli_diag_archive(int argc, char *argv[]);
int cli_diag_rotate(int argc, char *argv[]);  //placeholder if not used
int cli_diag_audit(int argc, char *argv[]);    //placeholder if not used
int cli_diag_log(int argc, char *argv[]);    //placeholder if not used
int cli_diag_dump(int argc, char *argv[]);
int cli_cppc_perf(int argc, char *argv[]);
int cli_cppc_bal(int argc, char *argv[]);
int cli_cppc_eco(int argc, char *argv[]);
int cli_cppc_power(int argc, char *argv[]);
int cli_cppc_epp(int argc, char *argv[]);
int cli_cppc_raw(int argc, char *argv[]);
int cli_cppc_mode(int argc, char *argv[]);
int cli_cppc_target(int argc, char *argv[]);
int cli_cppc_governor(int argc, char *argv[]);
int cli_cppc_prefcore(int argc, char *argv[]);
int cli_stress_cpu(int argc, char *argv[]);
int cli_misc_insults(int argc, char *argv[]);
int cli_misc_fallback(int argc, char *argv[]);

#endif // CLI_H
