/* Logging & Diagnostics for the X3D Toggle Project
 *
 * `diag.c` - Backend Migration
 *
 * IPC Command and Status Verifications
 */

#ifndef USR_LIBS
#define USR_LIBS "/usr/lib/x3d-toggle"
#endif

#include "../build/xui.h"
#include "cli.h"
#include "error.h"
#include "libc.h"
#include "modes.h"
#include "status.h"

const char *diag_ebpf(void) {
  static char bpf_path[BUFF_PATH];
  printf_sn(bpf_path, sizeof(bpf_path), "%s/bpf.o", USR_LIBS);
  if (access(bpf_path, F_OK) != 0) {
    printf_sn(bpf_path, sizeof(bpf_path), "%s/bpf.o", "/var/lib/x3d-toggle");
    if (access(bpf_path, F_OK) != 0) {
      if (access("build/bpf.o", F_OK) != 0) {
        return "MISSING (.O NOT FOUND)";
      }
      return "HEALTHY (DEV BUILD)";
    }
  }
  if (access(bpf_path, R_OK) != 0) {
    return "INACCESSIBLE (PERMISSION DENIED)";
  }
  return "HEALTHY";
}

void diag_restore(int signum) {
  (void)signum;
  cppc_restore();
  cli_set_dual();
}

void diag_failsafe(int sig) {
  syslog(LOG_CRIT,
         "FATAL: Emergency Failsafe Triggered by signal %d. Restoring hardware "
         "state...",
         sig);
  cppc_restore();
  diag_restore(sig);
}

int cli_diag_linter(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf_step(
      "${SEARCH} Initiating Project Audit: Launching automated linter suite...");

  pid_t pid = fork();
  if (pid == 0) {
    char linter_path[BUFF_PATH];
    printf_step("${ALRIGHT} Game process terminated successfully.");
    printf_sn(linter_path, sizeof(linter_path), "%s/scripts/tools/linter.sh", USR_LIBS);

    char *args[] = {(char *)"/usr/bin/sh", linter_path, NULL};
    char *env[] = {NULL};
    execve(args[0], args, env);
    write(2, "X3D Error: execve failed\n", 25);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return ERR_SUCCESS;
    } else {
      return ERR_IO;
    }
  } else {
    write(2, "X3D Error: fork failed\n", 23);
    return ERR_IO;
  }
  return ERR_IO;
}

// is this redundant code??
void launch_debug_observer(void) {
  pid_t pid = fork();
  if (pid == 0) {
    const char *debug_path = "/usr/lib/x3d-toggle/scripts/tools/debug.sh";
    char *args[] = {(char *)"/bin/sh", (char *)debug_path, NULL};
    char *env[] = {NULL};
    execve(args[0], args, env);
    _exit(1);
  }
}
int cli_diag_debug(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  printf_step("${SEARCH} Launching MVC Observer: Initializing real-time daemon and eBPF event monitor...");

  pid_t pid = fork();
  if (pid == 0) {
    char debug_path[BUFF_PATH];
    printf_sn(debug_path, sizeof(debug_path), "%s/scripts/tools/debug.sh", USR_LIBS);

    char *args[] = {(char *)"/usr/bin/sh", debug_path, NULL};
    char *env[] = {NULL};
    execve(args[0], args, env);
    write(2, "X3D Error: execve failed\n", 25);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return ERR_SUCCESS;
    } else {
      return ERR_IO;
    }
  } else {
    write(2, "X3D Error: fork failed\n", 23);
    return ERR_IO;
  }
  return ERR_IO;
}

int cli_diag_journal(int argc, char *argv[]) {
  if (argc < 3 || strcmp(argv[2], "-rotate") != 0) {
    journal_error(ERR_SYNTAX, "Usage: x3d-toggle journal-[type] -rotate");
    return ERR_SYNTAX;
  }

  const char *sub = argv[1];
  const char *flag = "--all";

  if (strcmp(sub, "journal-log") == 0)
    flag = "--log";
  else if (strcmp(sub, "journal-audit") == 0)
    flag = "--audit";
  else if (strcmp(sub, "journal-coredump") == 0)
    flag = "--coredump";

  printf_step("${WIPE} Initiating modular rotation of journals: %s...", flag);

  pid_t pid = fork();
  if (pid == 0) {
    char rotate_path[BUFF_PATH];
    printf_sn(rotate_path, sizeof(rotate_path), "%s/scripts/tools/rotate.sh", USR_LIBS);

    char *args[] = {(char *)"/usr/bin/sh", rotate_path, (char *)flag, NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, env);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
  }
  return ERR_IO;
}

int cli_diag_archive(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  printf_step("${PACKAGE} Finalizing diagnostics: Zipping all current session data...");

  pid_t pid = fork();
  if (pid == 0) {
    char archive_path[BUFF_PATH];
    printf_sn(archive_path, sizeof(archive_path), "%s/scripts/tools/archive.sh", USR_LIBS);

    char *args[] = {(char *)"/usr/bin/sh", archive_path, NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, env);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
  }
  return ERR_IO;
}

int cli_diag_dump(int argc, char *argv[]) {
  pid_t pid = fork();
  if (pid == 0) {
    char dump_path[BUFF_PATH];
    printf_sn(dump_path, sizeof(dump_path), "%s/scripts/tools/coredump.sh", USR_LIBS);

    char *editor = (argc > 2) ? argv[2] : (char *)"none";
    char *args[] = {(char *)"/usr/bin/sh", dump_path, editor, NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, env);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
  }
  return ERR_IO;
}

int cli_diag_rotate(int argc, char *argv[]) {
  (void)argc;
  const char *sub = argv[1];
  const char *flag = "--all";

  if (strcmp(sub, "rotate-log") == 0)
    flag = "--log";
  else if (strcmp(sub, "rotate-audit") == 0)
    flag = "--audit";

  printf_step("${WIPE} Initiating modular rotation of logs: %s...", flag);

  pid_t pid = fork();
  if (pid == 0) {
    char rotate_path[BUFF_PATH];
    printf_sn(rotate_path, sizeof(rotate_path), "%s/scripts/tools/rotate.sh", USR_LIBS);

    char *args[] = {(char *)"/usr/bin/sh", rotate_path, (char *)flag, NULL};
    char *env[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, env);
    _exit(1);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS : ERR_IO;
  }
  return ERR_IO;
}

/* end of DIAG.C */
