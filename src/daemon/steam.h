/* Steam Game Detection Helper for the X3D Toggle Project
 * `steam.h` - Header only, shared by polling.c and bpf-user.c
 *
 * Detects whether a given PID is a Steam game (not the Steam client itself)
 * by scanning /proc/<pid>/environ for SteamAppId.
 *
 * Steam injects SteamAppId=<appid> into game child processes only.
 * The Steam client and its helper processes (steamwebhelper, reaper,
 * pressure-vessel) never have this variable set.
 *
 * NOTE: Do NOT include libc.h here. All types and syscall wrappers
 * (open, read, close, strncmp, strlen, printf_sn) are assumed to be
 * in scope from the including translation unit. Pulling in libc.h here
 * conflicts with glibc headers (libbpf.h, sd-bus.h) in bpf-user.c.
 */

#ifndef STEAM_H
#define STEAM_H

/* Known Steam infrastructure process names that must NOT trigger detection.
 * These will never have SteamAppId set, but we guard against edge cases.
 * Kernel truncates comm to 15 chars — strncmp(comm, name, 15) handles that. */
static const char *const _steam_infra_names[] = {
    "steam",
    "steamwebhelper",
    "steamboot",
    "steam_osx",
    "reaper",
    "pressure-vessel",
    "SteamChildMonit",
    NULL
};

/* Returns 1 if the comm string matches a known Steam infrastructure process */
static inline int steam_is_infra_comm(const char *comm) {
    if (!comm || comm[0] == '\0') return 0;
    for (int i = 0; _steam_infra_names[i]; i++) {
        if (strncmp(comm, _steam_infra_names[i], 15) == 0)
            return 1;
    }
    return 0;
}

/* Returns 1 if the comm string is a known generic game engine thread/process */
static inline int is_game_engine_comm(const char *comm) {
    if (!comm || comm[0] == '\0') return 0;
    if (strncmp(comm, "GameThread", 10) == 0) return 1;
    return 0;
}


/* Primary detection function.
 * Returns 1 if pid_str is a Steam-launched game process:
 *   - Has SteamAppId=<nonzero> in /proc/<pid>/environ
 *   - /proc/<pid>/comm is not a known Steam infrastructure process name
 *
 * pid_str: ASCII decimal string (e.g. "12345") from /proc enumeration.
 * printf_sn is assumed to be declared by the including TU (via xui.h).
 */
static inline int scan_pid_steam(const char *pid_str) {
    if (!pid_str || pid_str[0] == '\0') return 0;

    char path[64], buf[4096];
    char comm[32] = {0};

    /* First check comm directly for infra (abort) or game engine (fast-path) */
    printf_sn(path, sizeof(path), "/proc/%s/comm", pid_str);
    int cfd = open(path, O_RDONLY);
    if (cfd >= 0) {
        ssize_t cn = read(cfd, comm, sizeof(comm) - 1);
        close(cfd);
        if (cn > 0) {
            comm[cn] = '\0';
            if (comm[cn - 1] == '\n') comm[cn - 1] = '\0';
            if (is_game_engine_comm(comm)) return 1;
            if (steam_is_infra_comm(comm)) return 0;
        }
    }

    /* Read /proc/<pid>/environ (NUL-separated KEY=VALUE pairs) */
    printf_sn(path, sizeof(path), "/proc/%s/environ", pid_str);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;


    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    /* Scan NUL-separated entries for SteamAppId=<nonzero> */
    ssize_t i = 0;
    while (i < n) {
        char *entry = buf + i;

        if (strncmp(entry, "SteamAppId=", 11) == 0) {
            const char *appid = entry + 11;
            /* Require non-empty and non-zero value */
            if (appid[0] != '\0' && !(appid[0] == '0' && appid[1] == '\0')) {
                return 1;
            }

        }

        /* Advance past NUL terminator to next entry */
        i += (ssize_t)strlen(entry) + 1;
    }
    return 0;
}

/* PID-integer variant: converts int pid to string then calls scan_pid_steam.
 * printf_sn is assumed in scope from the including TU. */
static inline int scan_pid_steam_by_pid(int pid) {
    if (pid <= 0) return 0;
    char pid_str[16];
    printf_sn(pid_str, sizeof(pid_str), "%d", pid);
    return scan_pid_steam(pid_str);
}

#endif /* STEAM_H */
