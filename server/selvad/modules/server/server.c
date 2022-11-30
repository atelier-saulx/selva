/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <netinet/tcp.h>
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
#include "jemalloc.h"
#include "selva_proto.h"
#define SELVA_SERVER_MAIN 1
#include "selva_server.h"
#include "server.h"

#define ENV_PORT_NAME "SELVA_PORT"
static int selva_port = 3000;
#define BACKLOG_SIZE 10
static const int use_tcp_nodelay = 1;
#define MAX_CLIENTS 100 /*!< Maximum number of client connections. */
static int server_sockfd;
static struct conn_ctx clients[MAX_CLIENTS];
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
    server_send_end(resp);
}

static void echo(struct selva_server_response_out *resp, const void *buf, size_t size) {
    struct selva_proto_string hdr;
    const char *p = (char *)buf;
    size_t left = size;

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

    server_send_end(resp);
}

static void cmdlist(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused) {
    selva_send_array(resp, -1);
    for (size_t i = 0; i < num_elem(commands); i++) {
        if (commands[i].cmd_fn) {
            selva_send_array(resp, 2);
            selva_send_ll(resp, i);
            selva_send_str(resp, commands[i].cmd_name, strlen(commands[i].cmd_name));
        }
    }
    selva_send_array_end(resp);

    server_send_end(resp);
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

static struct conn_ctx *alloc_conn_ctx(void)
{
    struct conn_ctx *ctx = NULL;

    /*
     * TODO We want to have greater max conns and thus foreach isn't good enough alloc
     */
    for (size_t i = 0; i < num_elem(clients); i++) {
        if (!clients[i].inuse) {
            ctx = &clients[i];
            ctx->inuse = 1;
            break;
        }
    }

    return ctx;
}

static void free_conn_ctx(struct conn_ctx *ctx)
{
    ctx->inuse = 0;
}

static void on_data(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct conn_ctx *ctx = (struct conn_ctx *)arg;

    /*
     * TODO Currently we don't do frame reassembly for multiple simultaneous
     *      sequences and expect the client to only send one message sequence
     *      at time.
     * TODO Some commands would possibly benefit from streaming support instead
     *      of buffering the whole request.
     */

    if (ctx->recv_state == CONN_CTX_RECV_STATE_NEW) {
        ctx->recv_msg_buf_i = 0;
    }

    ssize_t frame_bsize = server_recv_frame(ctx);
    if (frame_bsize <= 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed (fd: %d): %s",
                  fd, selva_strerror(frame_bsize));
        evl_end_fd(fd);
        return;
    }
    SELVA_LOG(SELVA_LOGL_INFO, "Received a frame of %d bytes from %d",
              (int)frame_bsize, fd);

    struct selva_proto_header *hdr = &ctx->recv_frame_hdr_buf;
    const uint32_t seqno = le32toh(hdr->seqno);
    const unsigned frame_state = hdr->flags & SELVA_PROTO_HDR_FFMASK;

    if (ctx->recv_state == CONN_CTX_RECV_STATE_NEW) {
        ctx->cur_seqno = seqno;
        size_t msg_bsize = le32toh(hdr->msg_bsize);

        if (!(frame_state & SELVA_PROTO_HDR_FFIRST)) {
            /* TODO Send an error */
            /* TODO Better log */
            SELVA_LOG(SELVA_LOGL_WARN, "Sequence tracking error: %d",
                      seqno);
            return;
        }

        /*
         * msg_bsize isn't necessarily set but if it is then we can alloc a
         * big enough buffer right away.
         */
        if (msg_bsize > SELVA_PROTO_MSG_SIZE_MAX) {
            /* TODO Send an error */
            return;
        } else if (ctx->recv_msg_buf_size < msg_bsize) {
            ctx->recv_msg_buf = selva_realloc(ctx->recv_msg_buf, msg_bsize);
            ctx->recv_msg_buf_size = msg_bsize;
        }
    } else if (ctx->recv_state == CONN_CTX_RECV_STATE_FRAGMENT) {
        if (seqno != ctx->cur_seqno) {
            SELVA_LOG(SELVA_LOGL_WARN, "Discarding an unexpected frame. seqno: %d", seqno);
            return;
        }
        if (frame_state & SELVA_PROTO_HDR_FFIRST) {
            SELVA_LOG(SELVA_LOGL_WARN, "Received invalid frame. seqno: %d", seqno);
            /* TODO Send an error? */
            ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
            return;
        }
    } else {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid connection state");
        return;
    }

    if (frame_state & SELVA_PROTO_HDR_FLAST) {
        selva_cmd_function cmd;
        struct selva_server_response_out resp = {
            .ctx = ctx,
            .cmd = hdr->cmd,
            .frame_flags = SELVA_PROTO_HDR_FFIRST,
            .seqno = seqno,
            .buf_i = 0,
        };

        cmd = get_command(hdr->cmd);
        if (cmd) {
            cmd(&resp, ctx->recv_msg_buf, ctx->recv_msg_buf_i);
        } else {
            const char msg[] = "Invalid command";

            SELVA_LOG(SELVA_LOGL_WARN, "%s: %d", msg, hdr->cmd);
            (void)selva_send_error(&resp, SELVA_PROTO_EINVAL, msg, sizeof(msg) - 1);
        }

        server_send_end(&resp);
        ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
    } else {
        ctx->recv_state = CONN_CTX_RECV_STATE_FRAGMENT;
    }
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

    if (use_tcp_nodelay) {
        (void)setsockopt(new_sockfd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
    }


    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s", buf);

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
    SELVA_MK_COMMAND(2, cmdlist);

    /* Async server for receiving messages. */
    server_sockfd = new_server(selva_port);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
