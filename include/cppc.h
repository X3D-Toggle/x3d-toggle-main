/* CPPC Subsystem Header for the X3D Toggle Project */
#ifndef CPPC_H
#define CPPC_H

int cppc_boost(int enable);
int cppc_tdp(int watts);
int cppc_perf(int val);
int cppc_epp(const char *val);
int cppc_ccd(int ccd, const char *val);
int cppc_pstate(const char *val);
int cppc_governor(const char *val);
int cppc_prefcore(int enable);
int cppc_restore(void);

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

#endif // CPPC_H
