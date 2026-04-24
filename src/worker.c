/* CLI Worker Transport Layer for the X3D Toggle Project
 *
 * `worker.c`
 *
 * Provides the transport mechanisms for CLI-to-Daemon communication.
 * Hardened with explicit stderr tracking to prevent silent UI failures.
 */

#include "ipc.h"
#include "error.h"
#include "systemd.h"

int socket_send(const char *cmd, char *response, size_t resp_len) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        journal_error(ERR_IPC, errno);
        return ERR_IPC;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    scat(addr.sun_path, IPC_PATH, sizeof(addr.sun_path));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == EACCES) {
            static int warned_group = 0;
            if (!warned_group) {
                journal_warn(ERR_MISSING);
                warned_group = 1;
            }
        } else if (errno == ENOENT) {
            if (unit_active()) {
                journal_warn(ERR_OFFLINE, IPC_PATH);
            }
        } else {
            journal_error(ERR_IPC, errno);
        }
        close(fd);
        return ERR_IPC;
    }

    if (send(fd, cmd, strlen(cmd), MSG_NOSIGNAL) < 0) {
        journal_error(ERR_IPC, errno);
        close(fd);
        return ERR_IPC;
    }

    if (response && resp_len > 0) {
        ssize_t bytes_read = read(fd, response, resp_len - 1);
        if (bytes_read > 0) {
            response[bytes_read] = '\0';
        } else {
            journal_error(ERR_IPC, (int)bytes_read);
            close(fd);
            return ERR_IPC;
        }
    } else {
        char buf[16] = {0};
        if (read(fd, buf, sizeof(buf) - 1) <= 0 || strncmp(buf, "OK", 2) != 0) {
            journal_error(ERR_IPC, -3);
            close(fd);
            return ERR_IPC;
        }
    }

    close(fd);
    return ERR_SUCCESS;
}

int socket_probe(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return ERR_IPC;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    scat(addr.sun_path, IPC_PATH, sizeof(addr.sun_path));

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR_IPC;
    }

    send(fd, "PING", 4, MSG_NOSIGNAL);
    char buf[16] = {0};
    int ret = (read(fd, buf, sizeof(buf)-1) > 0 && strncmp(buf, "OK", 2) == 0) ? ERR_SUCCESS : ERR_IPC;
    
    close(fd);
    return ret;
}

/* end of WORKER.C */
