/* Embedded Web Server for the X3D Toggle Project
 * `main.c` - Embedded web server implementation
 * Uses Embedded C Web Server Library
 * https://github.com/civetweb/civetweb
 */

#include "server.h"
#include "libc.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <unistd.h>
#include <arpa/inet.h>

int x3d_server_init(X3DServer *server, ServerConfig config);
void x3d_server_run(X3DServer *server);

/* end of MAIN.C */
