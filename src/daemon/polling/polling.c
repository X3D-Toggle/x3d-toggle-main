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
#include "steam.h"

#define _POSIX_C_SOURCE 202405L

extern volatile int active_override;
extern volatile int wrapper_lock;
extern DaemonConfig cfg;
extern bool bpf_active;
extern bool bpf_game(void);
extern void bpf_poll(int timeout_ms);

extern gamelist gl;

static bool scan_pid(const char *pid_str) {
    char path[256], buf[16384];

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

    /* GameMode detection: process has libgamemodeauto.so in its maps */
    printf_sn(path, sizeof(path), "/proc/%s/maps", pid_str);
    int maps_fd = open(path, O_RDONLY);
    if (maps_fd >= 0) {
        ssize_t n;
        while ((n = read(maps_fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            if (strstr(buf, "libgamemodeauto.so")) {
                close(maps_fd);
                return true;
            }
        }
        close(maps_fd);
    }

    /* Steam detection: process has SteamAppId set in its environment.
     * This covers Steam games launched without gamemoderun.
     * The Steam client itself never has SteamAppId, and we also guard
     * against known Steam infrastructure process names (steamwebhelper,
     * reaper, pressure-vessel, etc.) in scan_pid_steam(). */
    if (scan_pid_steam(pid_str))
        return true;

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
    
    if (wrapper_lock != 0) {
        printf_sn(target, 32, "cache");
        goto force_apply;
    }

    if (active_override < 3 && strcmp(cfg.daemon_state, "default") == 0) {
      int eg = 0, pg = 0;

      if (bpf_active) {
        bpf_poll(0);
      }
      eg = bpf_game();
      pg = detect_game();

      if (active_override == 1)
        printf_sn(target, 32, "cache");
      else if (active_override == 2)
        printf_sn(target, 32, "frequency");
      else
        printf_sn(target, 32, "%s", (eg || pg) ? "cache" : "frequency");

      target[31] = '\0';

      if (sysfs_ok && strcmp(current, target) != 0 && strlen(target) > 0) {
        cli_set_mode(target);
      }
    } else {
force_apply:
      if (sysfs_ok && strcmp(current, target) != 0 && strlen(target) > 0) {
        cli_set_mode(target);
      }
      printf_sn(target, 32, "%s", current);
      target[31] = '\0';
    }

    char display_target[32] = "";
    printf_sn(display_target, sizeof(display_target), "%s", target);

    if (active_override == 3)
      printf_sn(display_target, sizeof(display_target), "dual");
    if (active_override == 4)
      printf_sn(display_target, sizeof(display_target), "swap");

    static char last_target[32] = "";
    if (strcmp(display_target, last_target) != 0) {
      printf_sn(last_target, sizeof(last_target), "%s", display_target);
      journal_debug("State Transition: Target: %s | Mode: %s", 
                display_target, cfg.daemon_state);
    }
}

/* end of POLLING.C */
