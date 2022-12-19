#include <arpa/inet.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "server.h"

/* addr + port + nul */
#define CONN_STR_LEN (INET_ADDRSTRLEN + 5 + 1)
size_t conn_to_str(struct conn_ctx *ctx, char buf[CONN_STR_LEN], size_t bsize)
{
    struct sockaddr_in addr; /*!< Client/peer addr */
    socklen_t addr_size = sizeof(struct sockaddr_in);

    memset(buf, '\0', bsize); /* bc inet_ntop() may not terminate. */
    if (unlikely(bsize < CONN_STR_LEN)) {
        return 0;
    }

    if (getpeername(ctx->fd, (struct sockaddr *)&addr, &addr_size) == -1) {
        const int e = errno;

        static_assert(CONN_STR_LEN > 17);

        switch (e) {
        case ENOBUFS:
            strcpy(buf, "<sys error>");
            return 11;
        case EBADF:
        case ENOTCONN:
        case ENOTSOCK:
            strcpy(buf, "<not connected>");
            return 15;
        case EFAULT:
        case EINVAL:
        default:
            strcpy(buf, "<internal error>");
            return 16;
        }
    }

    if (!inet_ntop(AF_INET, &addr.sin_addr, buf, bsize)) {
        strcpy(buf, "<ntop failed>");
        return 13;
    }

    const ssize_t end = strlen(buf);
    const int n = bsize - end;
    const int res = snprintf(buf + end, n, ":%d", ntohs(addr.sin_port));

    return (res > 0 && res < n) ? end + n : end;
}

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
        ctx->recv_msg_buf_i = 0;
    }

    ssize_t frame_bsize = server_recv_frame(ctx);
    if (frame_bsize <= 0) {
        char peer[CONN_STR_LEN];

        conn_to_str(ctx, peer, sizeof(peer));
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed client: %s err: \"%s\"",
                  peer, selva_strerror(frame_bsize));
        return -1;
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
            return -1;
        }

        /*
         * msg_bsize isn't necessarily set but if it is then we can alloc a
         * big enough buffer right away.
         */
        if (msg_bsize > SELVA_PROTO_MSG_SIZE_MAX) {
            /* TODO Send an error instead of dropping the connection. */
            return -1;
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
            /* RFE Drop or send an error? */
            return 0;
        }
        if (frame_state & SELVA_PROTO_HDR_FFIRST) {
            char peer[CONN_STR_LEN];
            conn_to_str(ctx, peer, sizeof(peer));

            SELVA_LOG(SELVA_LOGL_WARN, "Received invalid frame. client: %s seqno: %d",
                      peer, seqno);
            /* TODO Send an error or drop? */
            ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
            return 0;
        }
    } else {
        char peer[CONN_STR_LEN];
        conn_to_str(ctx, peer, sizeof(peer));

        SELVA_LOG(SELVA_LOGL_ERR, "Invalid connection state. client: %s", peer);
        return -1; /* Drop the connection. */
    }

    if (!(frame_state & SELVA_PROTO_HDR_FLAST)) {
        ctx->recv_state = CONN_CTX_RECV_STATE_FRAGMENT;
        return 0;
    }

    ctx->recv_state = CONN_CTX_RECV_STATE_NEW;
    return 1;
}
