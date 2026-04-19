/* Telemetry and Status Header for the X3D Toggle Project */
#ifndef STATUS_H
#define STATUS_H

#include "error.h" // IWYU pragma: keep
#include "xui.h" // IWYU pragma: keep

typedef struct {
  unsigned long long user, nice, system, idle, iowait, irq, softirq;
} CPUStats;

typedef struct {
  char d_mode[BUFF_MODE];
  char daemon_state[BUFF_DAEMON];
  char ipc_status[BUFF_STATUS];
  char ebpf_status[BUFF_EBPF];
  char c_mode[BUFF_MODE];
  char ccd_state[BUFF_DISPLAY];
  char st_buff[BUFF_STATE];
  char st_buff_ccd0[BUFF_STATE];
  char st_buff_ccd1[BUFF_STATE];
  char d_buff[BUFF_DMODE];
  char epp[BUFF_EPP];
  char gov[BUFF_GOV];
  char smt[BUFF_SMT];
  char plat[BUFF_PLAT];
  double refresh_interval;

  const char *d_icon;
  const char *c_icon;
  const char *ccd_icon;
} Status;

int daemon_status(Status *st);
const char *diag_ebpf(void);
void stats(CPUStats *stats);
double cpu_load(CPUStats *prev, CPUStats *curr);

int cli_status_dialog(int argc, char *argv[]);
int cli_status_monitor(int argc, char *argv[]);
int cli_status_owl(int argc, char *argv[]);

#endif // STATUS_H
