/* Mode Execution Header for the X3D Toggle Project */
#ifndef MODES_H
#define MODES_H

#include "../build/config.h" // IWYU pragma: keep
#include "libc.h" // IWYU pragma: keep

int mode(char *current_mode, size_t max_len);
int mode_path(char *buf, size_t size);
int cli_set_mode(const char *target);
int cli_set_dual(void);
int cli_set_swap(void);
int cli_set_core(int core_id, int online);

int cli_mode_cache(int argc, char *argv[]);
int cli_mode_frequency(int argc, char *argv[]);
int cli_mode_dual(int argc, char *argv[]);
int cli_mode_swap(int argc, char *argv[]);
int cli_mode_auto(int argc, char *argv[]);
int cli_mode_default(int argc, char *argv[]);
int cli_mode_reset(int argc, char *argv[]);

#endif // MODES_H
