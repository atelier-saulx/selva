/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include "endian.h"
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "server.h"

int server_recv_message(struct conn_ctx *ctx)
{
    /*
     * TODO Currently we don't do frame reassembly for multiple simultaneous
     *      sequences and expect the client to only send one message sequence
     *      at time.
     * TODO Some commands would possibly benefit from streaming support instead
     *      of buffering the whole request.
     */

    if (ctx->recv_state == CONN_CTX_RECV_STATE_NEW) {
        /*
         * Reset the message buffer state.
         */
        ctx->recv_msg_buf_i = 0;
    }

    ssize_t frame_bsize = server_recv_frame(ctx);
    if (frame_bsize <= 0) {
        char peer[CONN_STR_LEN];

        conn_to_str(ctx, peer, sizeof(peer));
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed client: %s err: \"%s\"",
                  peer, selva_strerror(frame_bsize));
        return frame_bsize;
    }

    struct selva_proto_header *hdr = &ctx->recv_frame_hdr_buf;
    const uint32_t seqno = le32toh(hdr->seqno);
    const unsigned frame_state = hdr->flags & SELVA_PROTO_HDR_FFMASK;
    char peer[CONN_STR_LEN];

    conn_to_str(ctx, peer, sizeof(peer));
    SELVA_LOG(SELVA_LOGL_INFO, "Received a frame. client: %s seqno: %d bytes: %d",
              peer,
              (int)seqno,
              (int)frame_bsize);

    if (ctx->recv_state == CONN_CTX_RECV_STATE_NEW) {
        ctx->cur_seqno = seqno;
        size_t msg_bsize = le32toh(hdr->msg_bsize);

        /*
         * This is supposed to be the beginning of a new sequence.
         */
        if (!(frame_state & SELVA_PROTO_HDR_FFIRST)) {
            char peer[CONN_STR_LEN];

            conn_to_str(ctx, peer, sizeof(peer));
            SELVA_LOG(SELVA_LOGL_WARN, "Sequence tracking error client: %s seqno: %d",
                      peer,
                      seqno);
            /*
             * Drop the connection.
             * It's the easiest way because the client might be rogue or
             * in a broken state.
             */
            return SELVA_PROTO_EBADMSG;
        }

        /*
         * msg_bsize isn't necessarily set but if it is then we can alloc a
         * big enough buffer right away.
         */
        if (msg_bsize > SELVA_PROTO_MSG_SIZE_MAX) {
            return SELVA_PROTO_EBADMSG;
        } else if (ctx->recv_msg_buf_size < msg_bsize) {
            ctx->recv_msg_buf = selva_realloc(ctx->recv_msg_buf, msg_bsize);
            ctx->recv_msg_buf_size = msg_bsize;
        }
    } else if (ctx->recv_state == CONN_CTX_RECV_STATE_FRAGMENT) {
        if (seqno != ctx->cur_seqno) {
            char peer[CONN_STR_LEN];

            conn_to_str(ctx, peer, sizeof(peer));
            SELVA_LOG(SELVA_LOGL_WARN, "Discarding an unexpected frame. client: %s seqno: %d",
                      peer, seqno);
            /*
             * RFE Drop or send an error?
             * This is the point where we might want to do reassembly.
             */
            return 0;
        }
        if (frame_state & SELVA_PROTO_HDR_FFIRST) {
            char peer[CONN_STR_LEN];

            conn_to_str(ctx, peer, sizeof(peer));
            SELVA_LOG(SELVA_LOGL_WARN, "Received invalid frame. client: %s seqno: %d",
                      peer, seqno);
            ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
            return 0;
        }
    } else {
        char peer[CONN_STR_LEN];

        conn_to_str(ctx, peer, sizeof(peer));
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid connection state. client: %s", peer);
        return SELVA_PROTO_EBADMSG; /* Drop the connection. */
    }

    if (!(frame_state & SELVA_PROTO_HDR_FLAST)) {
        ctx->recv_state = CONN_CTX_RECV_STATE_FRAGMENT;
        return 0;
    }

    ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
    return 1;
}
