/* Server Module Header for the X3D Toggle Project
 *
 * `server.h` - Header only
 */

#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

typedef struct {
    char address[64];
    int port;
    bool enabled;
    char ssh_mode[16];
    char ssh_user[64];
    int ssh_port;
} ServerConfig;

typedef struct {
    int listen_fd;
    ServerConfig config;
} X3DServer;

int  x3d_server_init(X3DServer *server, ServerConfig config);
void x3d_server_run(X3DServer *server);
void x3d_server_stop(X3DServer *server);

#endif
