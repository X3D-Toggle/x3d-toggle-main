/* CLI Worker Transport Layer for the X3D Toggle Project
 * `worker.c`
 * Provides the transport mechanisms for CLI-to-Daemon communication.
 * Hardened with explicit stderr tracking to prevent silent UI failures.
 */

#include "ipc.h"
#include "error.h"
#include "systemd.h"
#include "xui.h"

int socket_handshake(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return ERR_IPC;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    printf_sn(addr.sun_path, sizeof(addr.sun_path), "%s", IPC_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR_IPC;
    }

    char hello[32];
    int len = printf_sn(hello, sizeof(hello), "HELLO %d", IPC_PROTOCOL_VERSION);
    if (send(fd, hello, (size_t)len, MSG_NOSIGNAL) != (ssize_t)len) {
        close(fd);
        return ERR_IPC;
    }

    char buf[64] = {0};
    ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes_read <= 0) {
        return ERR_IPC;
    }
    buf[bytes_read] = '\0';

    if (strncmp(buf, "OK ", 3) == 0) {
        return ERR_SUCCESS;
    }
    if (strncmp(buf, "ERR_VERSION ", 12) == 0) {
        char daemon_ver[32];
        printf_sn(daemon_ver, sizeof(daemon_ver), "v%s", buf + 12);
        journal_warn(ERR_IPC_VERSION, IPC_PROTOCOL_VERSION, daemon_ver);
        return ERR_IPC_VERSION;
    }
    if (strncmp(buf, "ERR", 3) == 0) {
        journal_warn(ERR_IPC_VERSION, IPC_PROTOCOL_VERSION, "legacy");
        return ERR_IPC_VERSION;
    }

    return ERR_IPC;
}

int socket_send(const char *cmd, char *response, size_t resp_len) {
    static int handshake_done = 0;
    if (!handshake_done && strncmp(cmd, "HELLO ", 6) != 0) {
        int hs = socket_handshake();
        if (hs == ERR_SUCCESS || hs == ERR_IPC_VERSION) {
            handshake_done = 1;
        }
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        journal_error(ERR_IPC, errno);
        return ERR_IPC;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    printf_sn(addr.sun_path, sizeof(addr.sun_path), "%s", IPC_PATH);

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
        } else if (bytes_read == 0) {
            journal_error(ERR_IPC, -3);
            close(fd);
            return ERR_IPC;
        } else {
            journal_error(ERR_IPC, errno);
            close(fd);
            return ERR_IPC;
        }
    } else {
        char buf[16] = {0};
        ssize_t bytes_read = read(fd, buf, sizeof(buf) - 1);
        if (bytes_read < 0) {
            journal_error(ERR_IPC, errno);
            close(fd);
            return ERR_IPC;
        }
        if (bytes_read == 0) {
            journal_error(ERR_IPC, ERR_IPC);
            close(fd);
            return ERR_IPC;
        }
        if (strncmp(buf, "OK", 2) != 0) {
            journal_error(ERR_IPC, ERR_IPC);
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
    printf_sn(addr.sun_path, sizeof(addr.sun_path), "%s", IPC_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return ERR_IPC;
    }

    ssize_t sent = send(fd, "PING", 4, MSG_NOSIGNAL);
    if (sent != 4) {
        close(fd);
        return ERR_IPC;
    }
    char buf[16] = {0};
    int ret = (read(fd, buf, sizeof(buf)-1) > 0 && strncmp(buf, "OK", 2) == 0) ? ERR_SUCCESS : ERR_IPC;
    
    close(fd);
    return ret;
}

/* end of WORKER.C */
