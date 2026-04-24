/* Game Launch wrapper for the X3D Toggle Project
 * `run.c`
 * This acts as a guaranteed fallback. It sets the v-Cache to cache mode
 * immediately before launch and restores it upon exit.
 * Usage: `x3d-run <executable> [args...]`
 * Steam Launch Options: `x3d-run %command%`

 */

#include "error.h"
#include "../build/xui.h"

extern char **environ;

int run_game(int argc, char *argv[]) {
  if (argc < 2) {
    journal_error(ERR_SYNTAX, "Usage: x3d-run [CMD] [ARGS...]");
    return ERR_SYNTAX;
  }

  char *cmd = (strcmp(argv[1], "run") == 0) ? argv[2] : argv[1];
  int start_idx = (strcmp(argv[1], "run") == 0) ? 2 : 1;

  if (!cmd) {
    journal_error(ERR_SYNTAX, "Usage: x3d-run [CMD] [ARGS...]");
    return ERR_SYNTAX;
  }

  printf_step("${HAMMER} Initiating Game Launcher: %s", cmd);

  pid_t pid = fork();
  if (pid < 0) {
    journal_error(ERR_IO, "Failed to fork for game execution");
    return ERR_IO;
  }

  if (pid == 0) {
    char *args[64];
    int arg_idx = 0;
    for (int i = start_idx; i < argc && arg_idx < 63; i++) {
      args[arg_idx++] = argv[i];
    }
    args[arg_idx] = NULL;

    execve(cmd, args, environ);

    char *sh_args[66];
    sh_args[0] = (char *)"/usr/bin/sh";
    sh_args[1] = (char *)"-c";
    sh_args[2] = cmd;
    int k = 3;
    for (int i = start_idx + 1; i < argc && k < 65; i++) {
      sh_args[k++] = argv[i];
    }
    sh_args[k] = NULL;

    execve(sh_args[0], sh_args, environ);

    write(2, "X3D Error: Execution failed\n", 28);
    _exit(1);
  } else {
    int status;
    waitpid(pid, &status, 0);

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
