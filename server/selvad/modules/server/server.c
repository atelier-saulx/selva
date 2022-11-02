/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "event_loop.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"

struct conn_ctx {
    int fd;
};

typedef void (*cmd_function)(struct conn_ctx *ctx, const char *buf, size_t size);

cmd_function commands[254];

#define BACKLOG_SIZE 3
static int server_sockfd;

static int mk_command(int nr, cmd_function cmd)
{
    if (nr < 0 || nr >= (int)num_elem(commands)) {
        return SELVA_EINVAL;
    }

    if (commands[nr]) {
        return SELVA_EEXIST;
    }

    commands[nr] = cmd;
    return 0;
}

static cmd_function get_command(int nr)
{
    return (nr >= 0 && nr < (int)num_elem(commands)) ? commands[nr] : NULL;
}

static void ping(struct conn_ctx *ctx, const char *buf, size_t size) {
    send(ctx->fd, "pong\r\n", 6, 0);
}

static int new_server(int port)
{
    int sockfd;
    struct sockaddr_in server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Could not create a socket");
        exit(EXIT_FAILURE);
    }

    (void)setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    (void)setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        SELVA_LOG(SELVA_LOGL_CRIT, "bind failed");
    }

    listen(sockfd, BACKLOG_SIZE);
    SELVA_LOG(SELVA_LOGL_INFO, "Listening on port: %d", port);

    return sockfd;
}

static void on_data(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    char buf[128];
    ssize_t r;

    r = read(fd, buf, sizeof(buf) - 1);
    if (r <= 0) {
        evl_end_fd(fd);
        return;
    }

    SELVA_LOG(SELVA_LOGL_INFO, "Received %d bytes", (int)r);

    struct selva_proto_header hdr;

    if (r < (ssize_t)sizeof(hdr)) {
        /* TODO */
        SELVA_LOG(SELVA_LOGL_ERR, "Header too small");
        return;
    }

    memcpy(&hdr, buf, sizeof(hdr));
    cmd_function cmd = get_command(hdr.cmd);
    if (cmd) {
        struct conn_ctx ctx = {
            .fd = fd,
        };

        cmd(&ctx, buf + sizeof(struct selva_proto_header), r - sizeof(struct selva_proto_header));
    } else {
        /* TODO with header */
        send(fd, "Invalid\r\n", 9, 0);
        return;
    }
}

static void on_connection(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int new_sockfd;
    char buf[INET_ADDRSTRLEN];

    new_sockfd = accept(fd, (struct sockaddr *)&client, (socklen_t*)&c);
    if (new_sockfd < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Accept failed");
        return;
    }

    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s", buf);

    evl_wait_fd(new_sockfd, on_data, NULL, NULL, NULL);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init server");

    mk_command(0, ping);

    /* Async server for receiving messages. */
    server_sockfd = new_server(3000);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
