/* Error Handling Logic Header for the X3D Toggle Project
 * `error.h` - Header only
 */

#ifndef ERROR_H
#define ERROR_H

typedef int error_code;

#define GAME_RESTORED      1
#define GAME_SYNCED        2
#define GAME_ADDED         3
#define GAME_REMOVED      15
#define PROFILE_ACTION     4
#define SET_EPP            5
#define SET_PSTATE         6
#define SET_CPPC           7
#define SET_GOV            8
#define TOGGLE_GOV         8
#define TOGGLE_PREFCORE    9
#define TOGGLE_BOOST      10
#define DAEMON_START      11
#define DAEMON_TRANSIT    13
#define TRANSIT           13
#define BPF_HIT           12
#define SOCKET_READY      14
#define STATE_ACTIVE      16
#define SET_XUI_SYNC      17

#define ERR_SUCCESS        0
#define ERR_LOST          -1
#define ERR_PERMISSION    -2
#define ERR_IO            -3
#define ERR_IPC           -4
#define ERR_HW            -5
#define ERR_SYNTAX        -6
#define ERR_POLKIT        -7
#define ERR_DAEMON        -8
#define ERR_CMD           -9

#define ERR_VIP          -10
#define ERR_MISSING      -11
#define ERR_PASSIVE      -12
#define ERR_PROFILE      -13
#define ERR_OFFLINE      -14
#define ERR_ADD          -15
#define ERR_REMOVE       -16
#define ERR_STATUS       -17
#define ERR_CPPC         -18
#define ERR_EPP          -19
#define ERR_GOV          -20
#define ERR_USAGE        -21
#define ERR_SOCKET       -22
#define ERR_BIND         -23
#define ERR_CHOWN        -24
#define ERR_CHMOD        -25
#define ERR_LISTEN       -26
#define ERR_LOAD         -27
#define ERR_LINTER       -28
#define ERR_MEM          -29
#define ERR_CONF         -30
#define ERR_SCAFFOLD     -31
#define ERR_PERM_BUILD   -32
#define ERR_SAFETY       -33
#define ERR_CGROUP       -34
#define ERR_AFFINITY     -35
#define ERR_GUI          -36
#define ERR_EXEC         -37

void journal_error(error_code code, ...);
void journal_warn(error_code code, ...);
void journal_info(error_code code, ...);
void buffer_set(void);
void buffer_flush(void);

void journal_write(int priority, const char *format, ...);
void journal_log(error_code code, ...);
void journal_exit(int exit_code, const char *format, ...);

void journal_debug(const char *format, ...);
void journal_diag(int level, const char *format, ...);
void journal_file(const char *level, const char *summary, const char *ctx);

const char *journal_string(int code);
void journal_syslog(int priority, const char *summary, const char *ctx);

#endif // ERROR_H
