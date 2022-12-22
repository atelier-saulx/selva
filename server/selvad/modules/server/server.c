/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "event_loop.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "jemalloc.h"
#include "selva_proto.h"
#define SELVA_SERVER_MAIN 1
#include "selva_server.h"
#include "tcp.h"
#include "server.h"

#define ENV_PORT_NAME "SELVA_PORT"
static int selva_port = 3000;
#define BACKLOG_SIZE 10 /* TODO Tunable? */
static int server_sockfd;

struct {
    selva_cmd_function cmd_fn;
    const char *cmd_name;
} commands[254];

int selva_mk_command(int nr, const char *name, selva_cmd_function cmd)
{
    if (nr < 0 || nr >= (int)num_elem(commands)) {
        return SELVA_EINVAL;
    }

    if (commands[nr].cmd_fn) {
        return SELVA_EEXIST;
    }

    commands[nr].cmd_fn = cmd;
    commands[nr].cmd_name = name;

    return 0;
}

static selva_cmd_function get_command(int nr)
{
    return (nr >= 0 && nr < (int)num_elem(commands)) ? commands[nr].cmd_fn : NULL;
}

static void ping(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused) {
    const char msg[] = "pong";

    selva_send_str(resp, msg, sizeof(msg) - 1);
}

static void echo(struct selva_server_response_out *resp, const void *buf, size_t size) {
    struct selva_proto_string hdr;
    const char *p = (char *)buf;
    size_t left = size;

    if (size == 0) {
        selva_send_errorf(resp, SELVA_EINVAL, "Empty payload");
        return;
    }

    /* TODO Could also support receiving an array */
    while (left > sizeof(hdr)) {
        size_t bsize;

        memcpy(&hdr, p, sizeof(hdr));
        left -= sizeof(hdr);
        p += sizeof(hdr);

        if (hdr.type != SELVA_PROTO_STRING) {
            const char err_str[] = "Invalid payload type";

            selva_send_error(resp, SELVA_EINVAL, err_str, sizeof(err_str) - 1);
            break;
        }

        bsize = le32toh(hdr.bsize);
        if (bsize > left) {
            const char err_str[] = "Invalid payload size";

            selva_send_error(resp, SELVA_EINVAL, err_str, sizeof(err_str) - 1);
            break;
        }

        selva_send_str(resp, p, bsize);
        left -= bsize;
        p += bsize;
    }
}

static void lscmd(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused) {
    selva_send_array(resp, -1);
    for (size_t i = 0; i < num_elem(commands); i++) {
        if (commands[i].cmd_fn) {
            selva_send_array(resp, 2);
            selva_send_ll(resp, i);
            selva_send_str(resp, commands[i].cmd_name, strlen(commands[i].cmd_name));
        }
    }
    selva_send_array_end(resp);
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
        exit(EXIT_FAILURE);
    }

    listen(sockfd, BACKLOG_SIZE);
    SELVA_LOG(SELVA_LOGL_INFO, "Listening on port: %d", port);

    return sockfd;
}

static void on_data(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct conn_ctx *ctx = (struct conn_ctx *)arg;
    int res;

    res = server_recv_message(ctx);
    if (res < 0) {
        /*
         * Drop the connection on error.
         * We can't send an error message because we don't know if the header
         * data is reliable.
         */
        evl_end_fd(fd);
    } else if (res == 1) {
        /* A message was received. */
        const uint32_t seqno = le32toh(ctx->recv_frame_hdr_buf.seqno);
        struct selva_server_response_out resp = {
            .ctx = ctx,
            .cmd = ctx->recv_frame_hdr_buf.cmd,
            .frame_flags = SELVA_PROTO_HDR_FFIRST,
            .seqno = seqno,
            .buf_i = 0,
        };
        selva_cmd_function cmd;

        cmd = get_command(resp.cmd);
        if (cmd) {
            cmd(&resp, ctx->recv_msg_buf, ctx->recv_msg_buf_i);
        } else {
            const char msg[] = "Invalid command";

            /* TODO Log client */
            SELVA_LOG(SELVA_LOGL_WARN, "%s: %d", msg, resp.cmd);
            (void)selva_send_error(&resp, SELVA_PROTO_EINVAL, msg, sizeof(msg) - 1);
        }

        server_send_end(&resp);
    }
    /* Otherwise we need to wait for more frames. */
}

static void on_close(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct conn_ctx *ctx = (struct conn_ctx *)arg;

    (void)shutdown(fd, SHUT_RDWR);
    close(fd);
    free_conn_ctx(ctx);
}

static void on_connection(struct event *event, void *arg __unused)
{
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int new_sockfd;
    char buf[INET_ADDRSTRLEN];
    struct conn_ctx *conn_ctx = alloc_conn_ctx();

    new_sockfd = accept(event->fd, (struct sockaddr *)&client, (socklen_t*)&c);
    if (new_sockfd < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Accept failed");
        return;
    }

    /*
     * TODO We want to dealloc this somewhere.
     * TODO connection timeout support?
     */
    if (!conn_ctx) {
        close(new_sockfd);
        SELVA_LOG(SELVA_LOGL_WARN, "Maximum number of client connections reached");
        return;
    }

    tcp_set_nodelay(new_sockfd);

    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s:%d", buf, ntohs(client.sin_port));

    conn_ctx->fd = new_sockfd;
    conn_ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
#if 0
    server_assign_worker(conn_ctx);
#endif

    evl_wait_fd(new_sockfd, on_data, NULL, on_close, conn_ctx);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
}

__constructor void init(void)
{
    const char *selva_port_str = getenv(ENV_PORT_NAME);

    SELVA_LOG(SELVA_LOGL_INFO, "Init server");

    if (selva_port_str) {
        selva_port = strtol(selva_port_str, NULL, 10);
    }

#if 0
    server_start_workers();
#endif

    SELVA_MK_COMMAND(0, ping);
    SELVA_MK_COMMAND(1, echo);
    SELVA_MK_COMMAND(2, lscmd);

    /* Async server for receiving messages. */
    server_sockfd = new_server(selva_port);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
