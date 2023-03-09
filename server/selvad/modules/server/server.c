/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jemalloc.h"
#include "endian.h"
#include "util/finalizer.h"
#include "util/net.h"
#include "util/selva_string.h"
#include "util/tcp.h"
#include "event_loop.h"
#include "config.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "../../tunables.h"
#include "server.h"

#define ENV_PORT_NAME "SELVA_PORT"
static int selva_port = 3000;
static int server_sockfd;
static int readonly_server;
static struct command {
    selva_cmd_function cmd_fn;
    enum selva_cmd_mode cmd_mode;
    const char *cmd_name;
} commands[254];

void selva_server_set_readonly(void)
{
    readonly_server = 1;
}

int selva_mk_command(int nr, enum selva_cmd_mode mode, const char *name, selva_cmd_function cmd)
{
    if (nr < 0 || nr >= (int)num_elem(commands)) {
        return SELVA_EINVAL;
    }

    if (commands[nr].cmd_fn) {
        return SELVA_EEXIST;
    }

    commands[nr].cmd_fn = cmd;
    commands[nr].cmd_mode = mode;
    commands[nr].cmd_name = name;

    return 0;
}

static struct command *get_command(int nr)
{
    return (nr >= 0 && nr < (int)num_elem(commands)) ? &commands[nr] : NULL;
}

size_t selva_resp_to_str(struct selva_server_response_out *resp, char *buf, size_t bsize)
{
    if (bsize < CONN_STR_LEN) {
        return 0;
    }

    if (!resp || resp->ctx) {
        strcpy(buf, "<not connected>");
        return 15;
    }

    return conn_to_str(resp->ctx, buf, bsize);
}

int selva_resp_to_cmd_id(struct selva_server_response_out *resp)
{
    return resp->cmd;
}

static void ping(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused)
{
    const char msg[] = "pong";

    selva_send_str(resp, msg, sizeof(msg) - 1);
}

static void echo(struct selva_server_response_out *resp, const void *buf, size_t size)
{
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

static void lscmd(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused)
{
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

static const struct timespec hrt_period = {
    .tv_sec = 5,
    .tv_nsec = 0,
};

static void hrt_cb(struct event *, void *arg)
{
    struct selva_server_response_out *resp = (struct selva_server_response_out *)arg;
    int tim;

#if 0
    SELVA_LOG(SELVA_LOGL_INFO, "Sending a heartbeat (%p, %d)", resp, resp->ctx ? resp->ctx->fd : -1);
#endif

    selva_send_str(resp, "boum", 4);

    if (selva_send_flush(resp)) {
        /* Connection reset. */
        (void)selva_send_end(resp);
        return;
    }

    tim = evl_set_timeout(&hrt_period, hrt_cb, resp);
    if (tim < 0) {
        (void)selva_send_errorf(resp, SELVA_ENOBUFS, "Failed to allocate a timer");
        (void)selva_send_end(resp);
        return;
    }

    resp->ctx->app.tim_hrt = tim;
}

static void hrt(struct selva_server_response_out *resp, const void *buf __unused, size_t size __unused)
{
    struct selva_server_response_out *stream_resp;
    int tim, err;

    if (resp->ctx->app.tim_hrt >= 0) {
        selva_send_errorf(resp, SELVA_EEXIST, "Already created");
        return;
    }

    err = selva_start_stream(resp, &stream_resp);
    if (err) {
        selva_send_errorf(resp, err, "Failed to create a stream");
        return;
    }

    tim = evl_set_timeout(&hrt_period, hrt_cb, stream_resp);
    if (tim < 0) {
        selva_cancel_stream(resp, stream_resp);
        selva_send_errorf(resp, tim, "Failed to create a timer");
        return;
    }

    resp->ctx->app.tim_hrt = tim;

    selva_send_ll(resp, 1);
}

/**
 * List config.
 * Resp:
 * [
 *   [
 *     mod_name,
 *     cfg_name,
 *     cfg_val,
 *     cfg_name,
 *     cfg_val,
 *     ...
 *   ],
 *   [
 *     mod_name,
 *     ...
 *   ]
 * ]
 */
static void config(struct selva_server_response_out *resp, const void *buf __unused, size_t size)
{
    const struct config_list *list;
    const size_t list_len = config_list_get(&list);

    if (size) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, list_len);
    for (size_t i = 0; i < list_len; i++) {
        const struct config *cfg_map = list[i].cfg_map;
        const size_t len = list[i].len;

        selva_send_array(resp, 1 + 2 * len);
        selva_send_strf(resp, "%s", list[i].mod_name);

        for (size_t j = 0; j < len; j++) {
            const struct config *cfg = &cfg_map[j];

            selva_send_strf(resp, "%s", cfg->name);
            switch (cfg->type) {
            case CONFIG_CSTRING:
                selva_send_strf(resp, "%s", *(char **)cfg->dp);
                break;
            case CONFIG_INT:
                selva_send_ll(resp, *(int *)cfg->dp);
                break;
            case CONFIG_SIZE_T:
                selva_send_ll(resp, *(size_t *)cfg->dp);
                break;
            default:
                selva_send_errorf(resp, SELVA_PROTO_ENOTSUP, "Unsupported type");
            }
        }
    }
}

static void mallocstats_send(void *arg, const char *buf)
{
    struct selva_server_response_out *resp = (struct selva_server_response_out *)arg;

    selva_send_strf(resp, "%s", buf);
}

static void mallocstats(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string *opts = NULL;
    int argc;

    finalizer_init(&fin);
    argc = selva_proto_scanf(&fin, buf, size, "%s", &opts);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc > 1) {
        selva_send_error_arity(resp);
        return;
    }

    selva_malloc_stats_print(mallocstats_send, resp, opts ? selva_string_to_str(opts, NULL) : NULL);
}

static void mallocprofdump(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string *filename = NULL;
    int argc;

    finalizer_init(&fin);
    argc = selva_proto_scanf(&fin, buf, size, "%s", &filename);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    } else if (argc > 1) {
        selva_send_error_arity(resp);
        return;
    }

    if (filename) {
        TO_STR(filename);

        selva_mallctl("prof.dump", NULL, NULL, (void *)&filename_str, sizeof(const char *));
    } else {
        selva_mallctl("prof.dump", NULL, NULL, NULL, 0);
    }

    selva_send_ll(resp, 1);
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

    listen(sockfd, SERVER_BACKLOG_SIZE);
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
            .cork = 1, /* Cork the full response. This will be turned of if a stream is started. */
            .cmd = ctx->recv_frame_hdr_buf.cmd,
            .frame_flags = SELVA_PROTO_HDR_FFIRST,
            .seqno = seqno,
            .buf_i = 0,
        };
        struct command *cmd;

        cmd = get_command(resp.cmd);
        if (cmd) {
            if (cmd->cmd_mode & SELVA_CMD_MODE_MUTATE && readonly_server) {
                static const char msg[] = "read-only server";

                (void)selva_send_error(&resp, SELVA_PROTO_ENOTSUP, msg, sizeof(msg) - 1);
            } else {
                cmd->cmd_fn(&resp, ctx->recv_msg_buf, ctx->recv_msg_buf_i);
            }
        } else {
            static const char msg[] = "Invalid command";

            (void)selva_send_error(&resp, SELVA_PROTO_EINVAL, msg, sizeof(msg) - 1);
        }

        if (!(resp.frame_flags & SELVA_PROTO_HDR_STREAM)) {
            selva_send_end(&resp);
        } /* The sequence doesn't end for streams. */
    }
    /* Otherwise we need to wait for more frames. */
}

static void on_close(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct conn_ctx *ctx = (struct conn_ctx *)arg;

    /*
     * This will also make async streams fail while we still keep the
     * the fd reserved, i.e. don't allow reusing the fd before we know
     * that no async function or a thread will try to write to it.
     */
    (void)shutdown(fd, SHUT_RDWR);

    free_conn_ctx(ctx);
}

static void on_connection(struct event *event, void *arg __unused)
{
    int c = sizeof(struct sockaddr_in);
    struct sockaddr_in client;
    int new_sockfd;
    char buf[INET_ADDRSTRLEN];
    struct conn_ctx *conn_ctx = alloc_conn_ctx();

    if (!conn_ctx) {
        SELVA_LOG(SELVA_LOGL_WARN, "Maximum number of client connections reached");
        return;
    }

    new_sockfd = accept(event->fd, (struct sockaddr *)&client, (socklen_t*)&c);
    if (new_sockfd < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Accept failed");
        return;
    }

    tcp_set_nodelay(new_sockfd);
    tcp_set_keepalive(new_sockfd, TCP_KEEPALIVE_TIME, TCP_KEEPALIVE_INTVL, TCP_KEEPALIVE_PROBES);

    inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a connection from %s:%d", buf, ntohs(client.sin_port));

    conn_ctx->fd = new_sockfd;
    conn_ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
    conn_ctx->app.tim_hrt = SELVA_EINVAL;
#if 0
    server_assign_worker(conn_ctx);
#endif

    evl_wait_fd(new_sockfd, on_data, NULL, on_close, conn_ctx);
}

void selva_server_run_cmd(int8_t cmd_id, void *msg, size_t msg_size)
{
    struct selva_server_response_out resp = {
        .ctx = NULL,
        .cmd = cmd_id,
    };
    struct command *cmd;

    cmd = get_command(cmd_id);
    if (cmd) {
        /*
         * Note that we don't care here whether the server is in read-only mode.
         */
        cmd->cmd_fn(&resp, msg, msg_size);
    } else {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid cmd_id");
    }
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(config_resolve);
    evl_import_main(config_list_get);
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

    SELVA_MK_COMMAND(CMD_PING_ID, SELVA_CMD_MODE_PURE, ping);
    SELVA_MK_COMMAND(CMD_ECHO_ID, SELVA_CMD_MODE_PURE, echo);
    SELVA_MK_COMMAND(CMD_LSCMD_ID, SELVA_CMD_MODE_PURE, lscmd);
    SELVA_MK_COMMAND(CMD_HRT_ID, SELVA_CMD_MODE_PURE, hrt);
    SELVA_MK_COMMAND(CMD_CONFIG_ID, SELVA_CMD_MODE_PURE, config);
    SELVA_MK_COMMAND(CMD_MALLOCSTATS_ID, SELVA_CMD_MODE_PURE, mallocstats);
    SELVA_MK_COMMAND(CMD_MALLOCPROFDUMP_ID, SELVA_CMD_MODE_PURE, mallocprofdump);

    /* Async server for receiving messages. */
    server_sockfd = new_server(selva_port);
    evl_wait_fd(server_sockfd, on_connection, NULL, NULL, NULL);
}
