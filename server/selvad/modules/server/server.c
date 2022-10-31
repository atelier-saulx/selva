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
#include "util/eztrie.h"
#include "module.h"
#include "selva_log.h"

struct conn_ctx {
    int fd;
};

typedef void (*cmd_function)(struct conn_ctx *ctx, const char *buf, size_t size);

#define BACKLOG_SIZE 3
static int server_sockfd;
static struct eztrie commands;

static int new_server(int port)
{
    int sockfd;
    struct sockaddr_in server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Could not create socket");
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

static void clear_crlf(char *buf)
{
    char *c = strpbrk(buf, "\r\n");

    if (c) {
        *c = '\0';
    }
}

static void on_data(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    char buf[128];
    ssize_t r;

    memset(buf, '\0', sizeof(buf));
    r = read(fd, buf, sizeof(buf) - 1);
    if (r <= 0) {
        evl_end_fd(fd);
        return;
    }

    clear_crlf(buf);
    SELVA_LOG(SELVA_LOGL_INFO, "Received msg: \"%.*s\"", (int)r, buf);

    if (!strcmp(buf, "end")) {
        /* Terminate this connection. */
        evl_end_fd(fd);
    } else if (!strcmp(buf, "quit")) {
        /* Stop the server. */
        evl_end_fd(fd);
        evl_end_fd(server_sockfd);
    } else {
        struct eztrie_iterator it;
        struct eztrie_node_value *v;

        it = eztrie_find(&commands, buf);
        v = eztrie_remove_ithead(&it);

        if (v && !strcmp(v->key, buf)) {
            struct conn_ctx ctx = {
                .fd = fd,
            };

            ((cmd_function)v->p)(&ctx, NULL, 0);
        } else {
            const char msg[] = "Unknown command\r\n";

            write(fd, msg, sizeof(msg) - 1);
        }
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

static void cmd_ping(struct conn_ctx *ctx, const char *buf, size_t size)
{
    write(ctx->fd, "pong\r\n", 6);
}

static void add_command(const char *name, cmd_function cmd)
{
    eztrie_insert(&commands, name, cmd);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init server");

    eztrie_init(&commands);
    add_command("ping", cmd_ping);

    /* Async server for receiving messages. */
    server_sockfd = new_server(3000);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
