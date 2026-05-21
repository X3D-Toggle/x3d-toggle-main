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

static void serve_ui(int client_fd, const ServerConfig *conf) {
    char response[2048];
    // Simple HTTP response serving the UI
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
        "<!DOCTYPE html><html><head><style>"
        "body { background: #0f0f0f; color: #00ff41; font-family: monospace; display: flex; "
        "justify-content: center; align-items: center; height: 100vh; margin: 0; }"
        ".matrix-box { border: 1px solid #00ff41; padding: 2rem; box-shadow: 0 0 15px #00ff41; }"
        "</style></head><body><div class='matrix-box'>"
        "<h1>X3D SERVER ACTIVE</h1>"
        "<p>IP: %s</p><p>PORT: %d</p><p>SSH MODE: %s</p>"
        "</div></body></html>",
        conf->address, conf->port, conf->ssh_mode);
    
    send(client_fd, response, strlen(response), 0);
}

int x3d_server_init(X3DServer *server, ServerConfig config) {
    server->config = config;
    struct sockaddr_in addr;

    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) return -1;

    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(config.address);
    addr.sin_port = htons(config.port);

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        return -1;
    }

    if (listen(server->listen_fd, 5) < 0) {
        close(server->listen_fd);
        return -1;
    }

    return 0;
}

void x3d_server_run(X3DServer *server) {
    printf("[X3D] Matrix listener active on %s:%d\n", 
           server->config.address, server->config.port);
    
    while (true) {
        int client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd < 0) continue;
        
        serve_ui(client_fd, &server->config);
        close(client_fd);
    }
}

/* end of MAIN.C */