/*
 * Selva Server Module.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define MAX_STREAMS 2

struct conn_ctx;

/**
 * Outgoing response.
 */
struct selva_server_response_out {
    struct conn_ctx *ctx;
    typeof_field(struct selva_proto_header, cmd) cmd;
    typeof_field(struct selva_proto_header, flags) frame_flags;
    typeof_field(struct selva_proto_header, seqno) seqno;
    size_t buf_i;
    _Alignas(struct selva_proto_header) char buf[SELVA_PROTO_FRAME_SIZE_MAX];
};

/**
 * Client connection descriptor.
 */
struct conn_ctx {
    int fd; /*<! The socket associated with this connection. */
    int inuse; /*!< Set if the connection is active. */
#if 0
    pthread_t worker_id;
#endif
    enum {
        CONN_CTX_RECV_STATE_NEW, /*!< Waiting for the next seq; No recv in progress. */
        CONN_CTX_RECV_STATE_FRAGMENT, /*!< Waiting for the next frame of a sequence. */
    } recv_state;
    typeof_field(struct selva_proto_header, seqno) cur_seqno; /*!< Currently incoming sequence. */
    /**
     * Application specific data.
     */
    struct {
        int tim_hrt; /*!< Server heartbeat timer. */
    } app;
    /**
     * Open streams.
     */
    struct {
        unsigned free_map; /*!< A bit is unset if the corresponding stream_resp is in use. */
        struct selva_server_response_out stream_resp[MAX_STREAMS];
    } streams;
    struct selva_proto_header recv_frame_hdr_buf;
    char *recv_msg_buf; /*!< Buffer for the currently incoming message. */
    size_t recv_msg_buf_size;
    size_t recv_msg_buf_i;
};

#if 0
void server_start_workers(void);
void server_dispatch2worker(struct conn_ctx *restrict ctx, const char *restrict payload, size_t payload_len);
#endif

/**
 * @addtogroup conn
 * Client connection.
 * Alloc, free, and describe client connections.
 * @{
 */

#ifdef INET_ADDRSTRLEN
/* addr + port + nul */
#define CONN_STR_LEN (INET_ADDRSTRLEN + 5 + 1)
#endif

/**
 * Allocate a new client connection descriptor.
 * Caller must set `ctx->fd`.
 */
[[nodiscard]]
struct conn_ctx *alloc_conn_ctx(void);

/**
 * Free a client connection descriptor.
 * This function will call `close(ctx->fd)` and it should not be closed before.
 */
void free_conn_ctx(struct conn_ctx *ctx);

/**
 * Allocate a stream_resp structure.
 * Note that it's the callers responsibility to initialize the returned struct.
 */
struct selva_server_response_out *alloc_stream_resp(struct conn_ctx *ctx);

/**
 * Free a stream_resp structure.
 */
void free_stream_resp(struct selva_server_response_out *stream_resp);

#ifdef INET_ADDRSTRLEN
/**
 * Describe a client connection.
 */
size_t conn_to_str(struct conn_ctx *ctx, char buf[CONN_STR_LEN], size_t bsize);
#endif

/**
 * @}
 */

int server_recv_message(struct conn_ctx *ctx);
ssize_t server_recv_frame(struct conn_ctx *ctx);
