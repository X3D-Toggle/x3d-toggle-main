/* IPC Interface for the X3D Toggle Project
 *
 * `ipc.h` - Header only
 */

#ifndef IPC_H
#define IPC_H

#include "libc.h" // IWYU pragma: keep


#define IPC_PATH "/run/x3d-toggle/x3d-toggle.ipc"
#define MAX_CONNECTIONS 128
#define IPC_GROUP "x3d-toggle"


int socket_send(const char *cmd, char *response, size_t resp_len);
int socket_probe(void);
int socket_setup(void);
void socket_handle(int server_fd);
void *socket_worker(void *arg);

void worker_start(void);
void worker_stop(void);

#endif // IPC_H

