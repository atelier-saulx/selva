/*
 * Selva Server Module.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define MAX_STREAMS 2

struct selva_server_response_out;
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
        _Atomic unsigned int free_map; /*!< A bit is unset if the corresponding stream_resp is in use. */
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
int server_flush_frame_buf(struct selva_server_response_out *resp, int last_frame);

/**
 * Send buffer as a part of the response resp.
 * The data is sent as is framed within selva_proto frames. Typically the buf
 * should point to one of the selva_proto value structs. The buffer might be
 * split into multiple frames and the receiver must reassemble the data. All
 * data within a sequence will be always delivered in the sending order.
 * @returns Return bytes sent; Otherwise an error.
 */
ssize_t server_send_buf(struct selva_server_response_out *restrict resp, const void *restrict buf, size_t len);

/**
 * Send contents of a file pointed by fd a part of the response resp.
 * The file is sent with a new selva_proto frame header with no payload but
 * msg_bsize set to size. The file is sent completely at once ignoring any
 * normal frame size limits. The frame header CRC check doesn't apply to the
 * file sent and thus any integrity checking must be implemented separately.
 * @returns Return bytes sent; Otherwise an error.
 */
ssize_t server_send_file(struct selva_server_response_out *resp, int fd, size_t size);
