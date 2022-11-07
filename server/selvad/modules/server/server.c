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
#include "event_loop.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "jemalloc.h"
#include "selva_proto.h"
#include "server.h"

selva_cmd_function commands[254];

#define BACKLOG_SIZE 3
static int server_sockfd;

static int mk_command(int nr, selva_cmd_function cmd)
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

static selva_cmd_function get_command(int nr)
{
    return (nr >= 0 && nr < (int)num_elem(commands)) ? commands[nr] : NULL;
}

static void ping(struct selva_server_response_out *resp, const char *buf __unused, size_t size __unused) {
    const char msg[] = "pong";

    printf("pong\n"); /* TODO remove */
    selva_send_str(resp, msg, sizeof(msg) - 1);
    server_send_end(resp); /* TODO end flag. */
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

static void on_data(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct conn_ctx *ctx = (struct conn_ctx *)arg;

    /*
     * TODO Currently we don't do frame reassembly and expect the client to only
     *      send one message sequence at time.
     */

    ssize_t frame_bsize = recv_frame(ctx);
    if (frame_bsize <= 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed (fd: %d): %s\n",
                  fd, selva_strerror(frame_bsize));
        evl_end_fd(fd);
        return;
    }
    SELVA_LOG(SELVA_LOGL_INFO, "Received a frame: %d bytes", (int)frame_bsize);

    struct selva_proto_header *hdr = &ctx->recv_frame_hdr_buf;
    const uint32_t seqno = le32toh(hdr->seqno);
    const unsigned frame_state = hdr->flags & SELVA_PROTO_HDR_FFMASK;

    if (ctx->recv_state == CONN_CTX_RECV_STATE_NONE ||
        ctx->recv_state == CONN_CTX_RECV_STATE_COMPLETE) {
        ctx->cur_seqno = seqno;
        size_t msg_bsize = le32toh(hdr->msg_bsize);

        if (!(frame_state & SELVA_PROTO_HDR_FFIRST)) {
            SELVA_LOG(SELVA_LOGL_WARN, "Sequence tracking error\n"); /* TODO Better log */
        }

        if (msg_bsize == 0) {
            msg_bsize = le16toh(hdr->frame_bsize - sizeof(*hdr));
        }

        ctx->recv_msg_buf = selva_realloc(ctx->recv_msg_buf, msg_bsize);
        ctx->recv_msg_buf_size = msg_bsize;
        ctx->recv_msg_buf_i = 0;
    } else if (ctx->recv_state == CONN_CTX_RECV_STATE_FRAGMENT) {
        if (seqno != ctx->cur_seqno) {
            SELVA_LOG(SELVA_LOGL_WARN, "Discarding an unexpected frame. seqno: %d\n", seqno);
            return;
        }
        if (frame_state & SELVA_PROTO_HDR_FFIRST) {
            SELVA_LOG(SELVA_LOGL_WARN, "Received invalid frame. seqno: %d\n", seqno);
            ctx->recv_state = CONN_CTX_RECV_STATE_NONE;
            return;
        }
    } else {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid connection state\n");
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

        ctx->recv_state = CONN_CTX_RECV_STATE_COMPLETE;

        cmd = get_command(hdr->cmd);
        if (cmd) {
            cmd(&resp, ctx->recv_msg_buf, ctx->recv_msg_buf_i);
        } else {
            const char msg[] = "Invalid command";

            SELVA_LOG(SELVA_LOGL_WARN, "%s: %d\n", msg, hdr->cmd);

            (void)selva_send_error(&resp, SELVA_PROTO_EINVAL, msg, sizeof(msg) - 1);
            server_send_end(&resp); /* TODO We want to add an end flag to selva_send.* */
            return;
        }
    }
}

static void on_connection(struct event *event, void *arg __unused)
{
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int new_sockfd;
    char buf[INET_ADDRSTRLEN];

    new_sockfd = accept(event->fd, (struct sockaddr *)&client, (socklen_t*)&c);
    if (new_sockfd < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Accept failed");
        return;
    }

    (void)setsockopt(new_sockfd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));

    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s", buf);

    /*
     * TODO We want to dealloc this somewhere.
     * TODO Perhaps static allocs for all connections.
     * TODO connection timeout support?
     */
    struct conn_ctx *conn_ctx = selva_calloc(1, sizeof(struct conn_ctx));

    conn_ctx->fd = new_sockfd;
    conn_ctx->recv_state = CONN_CTX_RECV_STATE_NONE;
#if 0
    server_assign_worker(conn_ctx);
#endif

    evl_wait_fd(new_sockfd, on_data, NULL, NULL, conn_ctx);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(selva_strerror);
    evl_import_event_loop();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init server");

#if 0
    server_start_workers();
#endif

    mk_command(0, ping);

    /* Async server for receiving messages. */
    server_sockfd = new_server(3000);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
