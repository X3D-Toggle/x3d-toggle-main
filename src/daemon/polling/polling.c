/* Polling Logic for the X3D Toggle Project
 * `x3d-polling.c`
 * Polling Detection Heuristics Logic for detecting games on system.
 * Acts as a fallback if the eBPF module fails to load or is unsupported.
 */

#include "polling.h"
#include "games.h"
#include "modes.h"
#include "status.h"
#include "xui.h"

#define _POSIX_C_SOURCE 202405L

extern volatile int active_override;
extern DaemonConfig cfg;
extern bool bpf_active;
extern bool bpf_game(void);
extern void bpf_poll(int timeout_ms);

extern gamelist gl;

static bool scan_pid(const char *pid_str) {
    char path[256], buf[1024];

    if (gl.count > 0) {
        printf_sn(path, sizeof(path), "/proc/%s/cmdline", pid_str);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) {
                buf[n] = '\0';
                for (ssize_t i = 0; i < n; i++) if (buf[i] == '\0') buf[i] = ' ';
                if (games_match(&gl, buf)) return true;
            }
        }
    }

    printf_sn(path, sizeof(path), "/proc/%s/maps", pid_str);
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            if (strstr(buf, "libgamemodeauto.so")) {
                close(fd);
                return true;
            }
        }
        close(fd);
    }

    return false;
}

bool detect_game(void) {
    int fd = open("/proc", O_RDONLY);
    if (fd < 0) return false;

    char buf[1024];
    int nread;
    struct linux_dirent64 *d;
    bool found = false;

    while ((nread = getdents64(fd, (struct linux_dirent64 *)buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread;) {
            d = (struct linux_dirent64 *)(buf + bpos);
            char c = d->d_name[0];
            if (c >= '0' && c <= '9') {
                if (scan_pid(d->d_name)) {
                    found = true;
                    break;
                }
            }
            bpos += d->d_reclen;
        }
        if (found) break;
    }

    close(fd);
    return found;
}

void polling_run(CPUStats *p_stat, CPUStats *c_stat, char *current, char *target) {
    stats(c_stat);
    (void)status_cpu(p_stat, c_stat);
    *p_stat = *c_stat;

    int sysfs_ok = (mode(current, 32) == ERR_SUCCESS);
    if (active_override < 3 && strcmp(cfg.daemon_state, "default") == 0) {
      int eg = 0, pg = 0;

      if (bpf_active) {
        bpf_poll(0);
        eg = bpf_game();
      } else {
        pg = detect_game();
      }

      if (active_override == 1)
        scat(target, "cache", 32);
      else if (active_override == 2)
        scat(target, "frequency", 32);
      else
        scat(target, (eg || pg) ? "cache" : "frequency", 32);

      target[31] = '\0';

      if (sysfs_ok && strcmp(current, target) != 0 && strlen(target) > 0) {
        cli_set_mode(target);
      }
    } else {
      scat(target, current, 32);
      target[31] = '\0';
    }

    char display_target[32] = "";
    scat(display_target, target, sizeof(display_target));

    if (active_override == 3)
      scat(display_target, "dual", sizeof(display_target));
    if (active_override == 4)
      scat(display_target, "swap", sizeof(display_target));

    static char last_target[32] = "";
    if (strcmp(display_target, last_target) != 0) {
      scat(last_target, display_target, sizeof(last_target));
      journal_debug("State Transition: Target: %s | Mode: %s", 
                display_target, cfg.daemon_state);
    }
}

/* end of POLLING.C */
