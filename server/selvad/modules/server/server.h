/*
 * Selva Server Module.
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#define MAX_STREAMS 2

struct selva_server_response_out;
struct conn_ctx;

typedef uint16_t pubsub_ch_mask_t;

/**
 * Outgoing response.
 */
struct selva_server_response_out {
    struct conn_ctx *ctx; /*!< Can be NULL. */
    int8_t cork; /*!< Cork the full response. Should not be used with streams. */
    typeof_field(struct selva_proto_header, cmd) cmd;
    typeof_field(struct selva_proto_header, flags) frame_flags;
    typeof_field(struct selva_proto_header, seqno) seqno;
    int last_error; /*!< Last error. Set by send_error functions. 0 if none. */
    int64_t ts; /* Timestamp when the command execution started. */
    size_t buf_i;
    _Alignas(struct selva_proto_header) char buf[SELVA_PROTO_FRAME_SIZE_MAX];
};

/**
 * Client connection descriptor.
 */
struct conn_ctx {
    int fd; /*<! The socket associated with this connection. */
    int8_t inuse; /*!< Set if the connection is active. */
    int8_t corked; /*!< Set if we have corked the socket. (avoids some unnecessary syscalls) */
    /**
     * Batch mode activated.
     * When set the server attempts to pack more responses together before
     * sending (uncorking the socket). This adds some latency to receiving the
     * responses but makes processing on the server-side more efficient.
     */
    int8_t batch_active;
    enum {
        CONN_CTX_RECV_STATE_NEW, /*!< Waiting for the next seq; No recv in progress. */
        CONN_CTX_RECV_STATE_FRAGMENT, /*!< Waiting for the next frame of a sequence. */
    } recv_state;
    typeof_field(struct selva_proto_header, seqno) cur_seqno; /*!< Currently incoming sequence. */
    /**
     * Application specific data.
     */
    pubsub_ch_mask_t pubsub_ch_mask; /*!< Subscribed to the channels in this mask. */
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
    alignas(uint64_t) struct selva_proto_header recv_frame_hdr_buf;
    char *recv_msg_buf; /*!< Buffer for the currently incoming message. */
    size_t recv_msg_buf_size;
    size_t recv_msg_buf_i;
};

enum server_send_flags {
    SERVER_SEND_MORE = 0x01,
};

/**
 * @addtogroup conn
 * Client connection.
 * Alloc, free, and describe client connections.
 * @{
 */

void conn_init(int max_clients);

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

void realloc_ctx_msg_buf(struct conn_ctx *ctx, size_t new_size);

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

/**
 * @addtogroup pubsub
 * Publish–subscribe.
 * @{
 */

void pubsub_init(void);

/**
 * Forcefully remove all streams belonging to ctx from pubsub.
 */
void pubsub_teardown(struct conn_ctx *ctx);

/**
 * Gracefully unsubscribe all pubsub streams.
 */
void pubsub_unsubscribe_all(struct conn_ctx *ctx);

/**
 * Unsubscribe ctx from channel.
 */
int pubsub_unsubscribe(struct conn_ctx *ctx, unsigned ch_id);

/**
 * @}
 */

/**
 * Receive a chuck of a message.
 * @returns <0 if receive failed; =0 if more frames are needed to reassemble the message; =1 if the message is now received completely.
 */
int server_recv_message(struct conn_ctx *ctx);

/**
 * Receive a single frame from a connection.
 */
ssize_t server_recv_frame(struct conn_ctx *ctx);

/**
 * Flush outgoing frame buffer.
 * Sends the data currently in the outgoing buffer.
 * @param last_frame if set the current message will be terminated.
 */
int server_flush_frame_buf(struct selva_server_response_out *resp, int last_frame);

/**
 * Cork the underlying socket.
 */
void server_cork_resp(struct selva_server_response_out *resp);

/**
 * Uncork the underlying socket.
 * The actual uncorking might not happen immediately if corking the socket is
 * requested through some other mean. E.g. batch processing.
 */
void server_uncork_resp(struct selva_server_response_out *resp);

/**
 * Send buffer as a part of the response resp.
 * The data is sent as is framed within selva_proto frames. Typically the buf
 * should point to one of the selva_proto value structs. The buffer might be
 * split into multiple frames and the receiver must reassemble the data. All
 * data within a sequence will be always delivered in the sending order.
 * @returns Return bytes sent; Otherwise an error.
 */
ssize_t server_send_buf(struct selva_server_response_out *restrict resp, const void *restrict buf, size_t len, enum server_send_flags flags);

/**
 * Send contents of a file pointed by fd a part of the response resp.
 * The file is sent with a new selva_proto frame header with no payload but
 * msg_bsize set to size. The file is sent completely at once ignoring any
 * normal frame size limits. The frame header CRC check doesn't apply to the
 * file sent and thus any integrity checking must be implemented separately.
 * @returns Return bytes sent; Otherwise an error.
 */
ssize_t server_send_file(struct selva_server_response_out *resp, int fd, size_t size, enum server_send_flags flags);
