/* Game Launch wrapper for the X3D Toggle Project
 * `run.c`
 * This acts as a guaranteed fallback. It sets the v-Cache to cache mode
 * immediately before launch and restores it upon exit.
 * Usage: `x3d-run <executable> [args...]`
 * Steam Launch Options: `x3d-run %command%`
 */

#include "libc.h"
#include "ipc.h"
#include "error.h"
#include "../build/xui.h"

extern char **environ;

int run_game(int argc, char *argv[]) {
  if (argc < 2) {
    journal_error(ERR_SYNTAX, "Usage: x3d-run [CMD] [ARGS...]");
    return ERR_SYNTAX;
  }

  int start_idx = (strcmp(argv[1], "run") == 0) ? 2 : 1;
  char *cmd = argv[start_idx];

  if (!cmd) {
    journal_error(ERR_SYNTAX, "Usage: x3d-run [CMD] [ARGS...]");
    return ERR_SYNTAX;
  }

  printf_step("${HAMMER} Initiating Game Launcher: %s", cmd);
  
  socket_send("SET_WRAPPER 1", NULL, 0);
  udelay(1000);

  pid_t pid = fork();
  if (pid < 0) {
    journal_error(ERR_IO, "Failed to fork for game execution");
    return ERR_IO;
  }

  if (pid == 0) {
    execvp(cmd, &argv[start_idx]);

    /* execvp only returns on failure — report via project error handler */
    journal_error(ERR_IO, cmd);
    _exit(1);
  } else {
    int status;
    waitpid(pid, &status, 0);

    socket_send("SET_WRAPPER 0", NULL, 0);

    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
      if (exit_code == 0) {
        printf_step("${ALRIGHT} Game process terminated successfully.");
      } else {
        printf_step("${XOUT} Game process exited with code %d.", exit_code);
      }
    } else if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      printf_step("${WARN} Game process killed by signal %d.", sig);
    }

    return ERR_SUCCESS;
  }
  return ERR_SUCCESS;
}

int daemon(int argc, char *argv[]) { return run_game(argc, argv); }

/* end of RUN.C */
