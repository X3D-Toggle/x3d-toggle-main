/* Shared Astraction Layer System Integration for the X3D Toggle Project
 *
 * `systemd.h` -Header only
 */

#ifndef SYSTEMD_H
#define SYSTEMD_H

//#include "error.h"
#include "modes.h" // IWYU pragma: keep

extern volatile sig_atomic_t reload_flag;
void init_signals(void);

void notify_ready(void);

void log_shutdown(void);

int unit_enable(void);
int unit_disable(void);
int unit_start(void);
int unit_stop(void);
int unit_active(void);

void daemon_restore(int signum);
void daemon_failsafe(int sig);

#endif // SYSTEMD_H
