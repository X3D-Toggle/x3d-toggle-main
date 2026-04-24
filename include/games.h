/* Shared Game List Loader for the X3D Toggle Project
 * `games.h` - Header only
 */

#ifndef GAMES_H
#define GAMES_H

#define GAMES_MAX     512
enum { GAME_NAME = 64 };

#define GAMES_SYS     "/etc/x3d-toggle.d"
#define GAMES_USR     "/etc/x3d-toggle.d"
#define GAMES_DEV     "./x3d-devgames.list"

typedef struct {
    char names[GAMES_MAX][GAME_NAME];
    int  count;
} gamelist;

int games_load(gamelist *gl);
int games_match(const gamelist *gl, const char *comm);
int game_add(const char *game);
int game_remove(const char *game);

#endif // GAMES_H
