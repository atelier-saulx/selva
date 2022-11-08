/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#if 0
#define SERVER_RING_BUF_BLOCK_SIZE  4096
#define SERVER_RING_BUF_LEN         128
#endif

/* TODO Perhaps extracts types from selva_proto with macros */

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
    uint32_t cur_seqno; /*!< Currently incoming sequence. */
    struct selva_proto_header recv_frame_hdr_buf;
    char *recv_msg_buf; /*!< Buffer for the currently incoming message. */
    size_t recv_msg_buf_size;
    size_t recv_msg_buf_i;
};

/**
 * Outgoing response.
 */
struct selva_server_response_out {
    struct conn_ctx *ctx;
    int8_t cmd;
    typeof_field(struct selva_proto_header, flags) frame_flags;
    uint32_t seqno;
    size_t buf_i;
    _Alignas(struct selva_proto_header) char buf[SELVA_PROTO_FRAME_SIZE_MAX];
};

/**
 * Command function.
 * @param resp contains information needed to build the response.
 * @param buf is a pointer to the incoming message.
 * @param len is the length of the incoming message in bytes.
 */
typedef void (*selva_cmd_function)(struct selva_server_response_out *resp, const char *buf, size_t len);

#if 0
void server_start_workers(void);
void server_dispatch2worker(struct conn_ctx *restrict ctx, const char *restrict payload, size_t payload_len);
#endif

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
 * Flush the response buffer.
 */
int server_send_flush(struct selva_server_response_out *restrict res);

/**
 * End sending a response.
 * Finalizes the response sequence.
 */
int server_send_end(struct selva_server_response_out *restrict res);

/* TODO These should be in a public API */

/**
 * Send an error.
 * @param msg_str can be NULL.
 */
int selva_send_error(struct selva_server_response_out *resp, int err, const char *msg_str, size_t msg_len);

int selva_send_double(struct selva_server_response_out *resp, double value);
int selva_send_ll(struct selva_server_response_out *resp, long long value);
int selva_send_str(struct selva_server_response_out *resp, const char *str, size_t len);

/**
 * If `len` is set negative then selva_proto_send_array_end() should be used to
 * terminate the array.
 * @param len Number if items in the array.
 */
int selva_send_array(struct selva_server_response_out *resp, int len);

int selva_send_array_end(struct selva_server_response_out *res);

ssize_t recv_frame(struct conn_ctx *ctx);
