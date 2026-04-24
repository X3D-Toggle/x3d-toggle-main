/* Abstraction Layer Systemd Integration for the X3D Toggle Project
 * `systemd.c`
 * System wrapper that handles the volatile flag management of the backend.
 * Utilizes direct systemctl execution via fork/execve.
 */

#include "systemd.h"
#include "cppc.h"
#include "daemon.h"
#include "error.h"

#define SERVICE_UNIT "x3d-toggle.service"
#ifndef VAR_LOGS
#define VAR_LOGS "/var/log/x3d-toggle/logs"
#endif
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif

extern char **environ;

static void redirect_systemctl_stdio(void) {
  int null_fd = open("/dev/null", O_WRONLY);
  if (null_fd >= 0) {
    dup2(null_fd, 1);
    close(null_fd);
  }

  int log_fd = open(VAR_LOGS "/systemd-exec.log", O_WRONLY | O_CREAT | O_APPEND,
                    0664);
  if (log_fd >= 0) {
    dup2(log_fd, 2);
    close(log_fd);
  }
}

static int execute_unit_method(const char *method) {
  pid_t pid = fork();
  if (pid < 0)
    return ERR_IO;

  if (pid == 0) {
    redirect_systemctl_stdio();
    char *args[] = {(char *)"/usr/bin/systemctl", (char *)method,
                    (char *)SERVICE_UNIT, NULL};
    execve(args[0], args, environ);
    _exit(EXIT_FAILURE);
  } else {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? ERR_SUCCESS
                                                           : ERR_IPC;
  }
  return ERR_IPC;
}

int unit_start(void) { return execute_unit_method("start"); }
int unit_stop(void) { return execute_unit_method("stop"); }
int unit_enable(void) { return execute_unit_method("enable"); }
int unit_disable(void) { return execute_unit_method("disable"); }

int unit_active(void) {
  pid_t pid = fork();
  if (pid < 0)
    return 0;

  if (pid == 0) {
    redirect_systemctl_stdio();
    char *args[] = {(char *)"/usr/bin/systemctl", (char *)"is-active",
                    (char *)SERVICE_UNIT, NULL};
    execve(args[0], args, environ);
    _exit(EXIT_FAILURE);
  } else {
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
  }
  return 0;
}

volatile int active_override = 0;
volatile sig_atomic_t active_keep = 1;
volatile sig_atomic_t last_sig = 0;
volatile sig_atomic_t reload_flag = 0;

static void sig_handler(int signum) {
  last_sig = signum;
  if (signum == SIGTERM || signum == SIGINT) {
    active_keep = 0;
  } else if (signum == SIGHUP) {
    reload_flag = 1;
  } else {
    daemon_failsafe(signum);
    _exit(signum);
  }
}

void init_signals(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigemptyset(&(sa.sa_mask));
  sa.sa_flags = SA_RESTART;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);
}

void notify_ready(void) {
  const char *sock_path = getenv("NOTIFY_SOCKET");
  if (!sock_path)
    return;

  int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return;

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  size_t path_len = strlen(sock_path);
  if (path_len >= sizeof(addr.sun_path)) {
    path_len = sizeof(addr.sun_path) - 1;
  }

  if (sock_path[0] == '@') {
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, sock_path + 1, path_len - 1);
  } else {
    memcpy(addr.sun_path, sock_path, path_len);
  }

  socklen_t addr_len = offsetof(struct sockaddr_un, sun_path) + path_len;

  const char *msg = "READY=1\n";
  (void)sendto(fd, msg, strlen(msg), MSG_NOSIGNAL, (struct sockaddr *)&addr,
               addr_len);
  close(fd);
}

void log_shutdown(void) {
  char buf[128];
  printf_sn(buf, 128, "Shutting down daemon... (Signal: %d)\n", (int)last_sig);
  write(2, buf, strlen(buf));
}

void daemon_restore(int signum) {
  (void)signum;
  cppc_restore();

  pid_t pid = fork();
  if (pid == 0) {
    char *args[] = {(char *)"/bin/sh",
                    (char *)"/usr/lib/x3d-toggle/scripts/tools/reset.sh",
                    NULL};
    char *envp[] = {(char *)"X3D_EXEC=1", NULL};
    execve(args[0], args, envp);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    int status;
    (void)waitpid(pid, &status, 0);
  }
}

void daemon_failsafe(int sig) {
  syslog(LOG_CRIT,
         "FATAL: Emergency Failsafe Triggered by signal %d. Restoring hardware "
         "state...",
         sig);
  journal_file("FATAL", "FAILSAFE", "Signal Triggered Emergency Restore");

  cppc_restore();
  daemon_restore(sig);
}

/* end of SYSTEMD.C */
