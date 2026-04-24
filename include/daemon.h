/* Daemon Lifecycle and Failsafe Header for the X3D Toggle Project
 * `daemon.h` - Header only
 */

#ifndef DAEMON_H
#define DAEMON_H

void daemon_restore(int signum);
void daemon_failsafe(int sig);

int cli_daemon_start(int argc, char *argv[]);
int cli_daemon_enable(int argc, char *argv[]);
int cli_daemon_wake(int argc, char *argv[]);
int cli_daemon_sleep(int argc, char *argv[]);
int cli_daemon_stop(int argc, char *argv[]);
int cli_daemon_disable(int argc, char *argv[]);

int cli_framework(int argc, char *argv[]);
int daemon(int argc, char *argv[]);

#endif // DAEMON_H
