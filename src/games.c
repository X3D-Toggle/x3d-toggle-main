/* Shared Game List Loader for the X3D Toggle Project
 *
 * `games.c`
 *
 * Implements the list parsing and matching logic for application-aware
 * CCD selection. Shared by both the daemon and CLI.
 */

#include "../build/xui.h"
#include "../build/config.h"
#include "../include/games.h"
#include "../include/error.h"

int games_load(gamelist *gl)
{
    if (!gl) return 0;
    gl->count = 0;
    
    int fd = open(GAMES_SYS, O_RDONLY);
    if (fd < 0) return 0;

    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return 0; }
    buf[n] = '\0';
    close(fd);

    char *line = buf;
    char *next;
    int game_section = 0;
    while (line && *line && gl->count < GAMES_MAX) {
        next = strchr(line, '\n');
        if (next) *next = '\0';

        if (line[0] == '[') {
            if (strstr(line, "[GAMES_SYS]") || strstr(line, "[GAMES_USR]")) {
                game_section = 1;
            } else {
                game_section = 0;
            }
            goto skip_line;
        }
        if (!game_section) goto skip_line;

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
            line[len - 1] == ' '  || line[len - 1] == '\t'))
            len--;
        line[len] = '\0';

        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#' || strlen(p) < 2) continue;

        int dup = 0;
        for (int i = 0; i < gl->count; i++) {
            if (strcmp(gl->names[i], p) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        size_t plen = strlen(p);
        if (plen >= GAME_NAME) plen = GAME_NAME - 1;
        memcpy(gl->names[gl->count], p, plen);
        gl->names[gl->count][plen] = '\0';
        gl->count++;

skip_line:
        if (next) line = next + 1;
        else break;
    }
    return gl->count;
}

int games_match(const gamelist *gl, const char *comm)
{
    if (!gl || !comm || comm[0] == '\0') return 0;
    for (int i = 0; i < gl->count; i++) {
        if (gl->names[i][0] == '\0') continue;
        if (strstr(comm, gl->names[i])) return 1;
    }
    return 0;
}

int game_add(const char *game) {
    if (!game) return ERR_SYNTAX;
    int fd = open(CONFIG_PATH, O_RDONLY);
    char lines[1024][256];
    int count = 0, found_usr = 0, found_sys = 0;
    int games_usr_idx = -1;

    if (fd >= 0) {
        char buf[16384];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *ln = buf;
            char *nxt;
            int current_section = 0;
            while (ln && *ln && count < 1024) {
                nxt = strchr(ln, '\n');
                if (nxt) *nxt = '\0';
                scat(lines[count], ln, 255);
                scat(lines[count], "\n", 256);

                if (ln[0] == '[') {
                    if (strstr(ln, "[GAMES_SYS]")) current_section = 1;
                    else if (strstr(ln, "[GAMES_USR]")) {
                        current_section = 2;
                        games_usr_idx = count;
                    } else current_section = 0;
                } else {
                    char clean[256];
                    scat(clean, ln, sizeof(clean));
                    if (current_section == 1 && strcmp(clean, game) == 0) found_sys = 1;
                    if (current_section == 2 && strcmp(clean, game) == 0) found_usr = 1;
                }
                count++;
                if (nxt) ln = nxt + 1;
                else break;
            }
        }
        close(fd);
    }

    if (found_sys || found_usr) {
        journal_info(GAME_ADDED, game, CONFIG_PATH);
        return ERR_SUCCESS;
    }

    if (games_usr_idx == -1) {
        printf_sn(lines[count++], 256, "\n[GAMES_USR]\n%s\n", game);
    } else {
        for (int j = count; j > games_usr_idx + 1; j--) {
            if (j < 1024) scat(lines[j], lines[j-1], 256);
        }
        printf_sn(lines[games_usr_idx + 1], 256, "%s\n", game);
        count++;
    }

    int out = open(CONFIG_PATH, O_WRONLY | O_TRUNC | O_CREAT, 0664);
    if (out >= 0) {
        for (int i = 0; i < count; i++) write(out, lines[i], strlen(lines[i]));
        close(out);
    }
    journal_info(GAME_ADDED, game, CONFIG_PATH);
    return ERR_SUCCESS;
}

int game_remove(const char *game) {
    if (!game) return ERR_SYNTAX;
    int fd = open(CONFIG_PATH, O_RDONLY);
    char lines[1024][256];
    int count = 0, found_usr = 0, found_sys = 0;

    if (fd >= 0) {
        char buf[16384];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            char *ln = buf;
            char *nxt;
            int current_section = 0;
            while (ln && *ln && count < 1024) {
                nxt = strchr(ln, "\n"[0]);
                if (nxt) *nxt = '\0';
                scat(lines[count], ln, 255);
                scat(lines[count], "\n", 256);

                if (ln[0] == '[') {
                    if (strstr(ln, "[GAMES_SYS]")) current_section = 1;
                    else if (strstr(ln, "[GAMES_USR]")) current_section = 2;
                    else current_section = 0;
                } else {
                    char clean[256];
                    scat(clean, ln, sizeof(clean));
                    if (current_section == 1 && strcmp(clean, game) == 0) found_sys = 1;
                    if (current_section == 2 && strcmp(clean, game) == 0) found_usr = 1;
                }
                count++;
                if (nxt) ln = nxt + 1;
                else break;
            }
        }
        close(fd);
    }

    if (found_sys) {
        journal_warn(ERR_REMOVE, game);
        printf_string("Error: Cannot remove system-provided game defaults.");
        return ERR_REMOVE;
    }
    if (!found_usr) {
        journal_warn(ERR_REMOVE, game);
        return ERR_REMOVE;
    }

    int new_count = 0;
    char new_lines[1024][256];
    int in_usr = 0;
    for (int i = 0; i < count; i++) {
        if (strstr(lines[i], "[GAMES_USR]")) in_usr = 1;
        else if (lines[i][0] == '[') in_usr = 0;

        if (in_usr) {
            char clean[256];
            scat(clean, lines[i], sizeof(clean));
            if (strcmp(clean, game) == 0) continue;
        }
        scat(new_lines[new_count++], lines[i], 256);
    }

    int out = open(CONFIG_PATH, O_WRONLY | O_TRUNC | O_CREAT, 0664);
    if (out >= 0) {
        for (int i = 0; i < new_count; i++) write(out, new_lines[i], strlen(new_lines[i]));
        close(out);
    }
    journal_info(GAME_REMOVED, game, CONFIG_PATH);
    return ERR_SUCCESS;
}

int game_action(int argc, char *argv[]) {
    if (argc < 3) {
        journal_error(ERR_SYNTAX, argv[1]);
        return ERR_SYNTAX;
    }
    if (strcmp(argv[1], "game-add") == 0) return game_add(argv[2]);
    return game_remove(argv[2]);
}

/* end of GAMES.C */
