/*
 * Selva Server Module.
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#if 0
#define SERVER_RING_BUF_BLOCK_SIZE  4096
#define SERVER_RING_BUF_LEN         128
#endif

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
    typeof_field(struct selva_proto_header, cmd) cmd;
    typeof_field(struct selva_proto_header, flags) frame_flags;
    typeof_field(struct selva_proto_header, seqno) seqno;
    size_t buf_i;
    _Alignas(struct selva_proto_header) char buf[SELVA_PROTO_FRAME_SIZE_MAX];
};

#if 0
void server_start_workers(void);
void server_dispatch2worker(struct conn_ctx *restrict ctx, const char *restrict payload, size_t payload_len);
#endif

int server_recv_message(struct conn_ctx *ctx);
ssize_t server_recv_frame(struct conn_ctx *ctx);
