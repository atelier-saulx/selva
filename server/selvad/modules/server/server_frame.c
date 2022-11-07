/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "endian.h"
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "server.h"

#define MAX_RETRIES 3

static int send_frame(int sockfd, const void *buf, size_t len, int flags)
{
    int retry_count = 0;
    ssize_t res;

retry:
    res = send(sockfd, buf, len, flags);
    if (res < 0) {
        switch (errno) {
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
        case ENOBUFS:
            /* TODO Should we sleep here? */
            if (retry_count++ > MAX_RETRIES) {
                return SELVA_PROTO_ENOBUFS; /* Not quite exact for EAGAIN but good enough. */
            }
            goto retry;
        case EINTR:
            goto retry;
        case EBADF:
            return SELVA_PROTO_EBADF;
        case ENOMEM:
            return SELVA_PROTO_ENOMEM;
        case ECONNRESET:
            return SELVA_PROTO_ECONNRESET;
        case ENOTCONN:
            return SELVA_PROTO_ENOTCONN;
        case ENOTSUP:
#if ENOTSUP != EOPNOTSUPP
        case EOPNOTSUPP:
#endif
            return SELVA_PROTO_ENOTSUP;
        default:
            return SELVA_PROTO_EINVAL;
        }
    }
    return 0;
}

static void start_resp_frame_buf(struct selva_server_response_out *resp)
{
    struct selva_proto_header *hdr = (struct selva_proto_header *)resp->buf;

    /* Make sure it's really zeroed as initializers might leave some bits. */
    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = resp->cmd;
    hdr->flags = SELVA_PROTO_HDR_FREQ_RES | resp->frame_flags;
    hdr->seqno = htole32(resp->seqno);

    resp->buf_i = sizeof(*hdr);
    resp->frame_flags &= ~SELVA_PROTO_HDR_FFMASK;
}

static void finalize_frame(char *buf, size_t bsize, int last_frame)
{
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    hdr->flags |= last_frame ? SELVA_PROTO_HDR_FLAST : 0;
    hdr->frame_bsize = htole16(bsize);
    hdr->chk = 0;

    /* TODO calc */
#if 0
    chk = htole32(chk);
#endif
}

static int flush_frame_buf(struct selva_server_response_out *resp, int last_frame)
{
    int err;

    assert(resp->buf_i >= sizeof(struct selva_proto_header));

    finalize_frame(resp->buf, resp->buf_i, last_frame);
    err = send_frame(resp->ctx->fd, resp->buf, resp->buf_i, 0);
    resp->buf_i = 0;

    return err;
}

ssize_t server_send_buf(struct selva_server_response_out *restrict resp, const void *restrict buf, size_t len)
{
    size_t i = 0;
    ssize_t ret = (ssize_t)len;

    if (!resp->ctx) {
        return SELVA_PROTO_EINVAL;
    }

    (void)setsockopt(resp->ctx->fd, IPPROTO_TCP, TCP_CORK, &(int){1}, sizeof(int));
    while (i < len) {
        if (resp->buf_i >= sizeof(resp->buf)) {
            int err;
            err = flush_frame_buf(resp, 0);
            if (err) {
                ret = err;
                goto out;
            }
        }
        if (resp->buf_i == 0) {
            start_resp_frame_buf(resp);
        }

        const size_t wr = min(sizeof(resp->buf) - resp->buf_i, len - i);
        memcpy(resp->buf + resp->buf_i, (uint8_t *)buf + i, wr);
        i += wr;
        resp->buf_i += wr;
    }

out:
    (void)setsockopt(resp->ctx->fd, IPPROTO_TCP, TCP_CORK, &(int){0}, sizeof(int));
    return ret;
}

int server_send_flush(struct selva_server_response_out *restrict resp)
{
    if (!resp->ctx) {
        return SELVA_PROTO_EINVAL;
    }

    return flush_frame_buf(resp, 0);
}

int server_send_end(struct selva_server_response_out *restrict resp)
{
    int err;

    if (!resp->ctx) {
        return SELVA_PROTO_EINVAL;
    }

    err = flush_frame_buf(resp, 1);

    resp->ctx = NULL; /* Make sure nothing will be sent anymore. */
    return err;
}

ssize_t recv_frame(struct conn_ctx *ctx)
{
    int fd = ctx->fd;
    ssize_t r;
    ssize_t frame_bsize;

    /* TODO We might want to do this in a single read and add more buffering to reduce syscall overhead. */
    r = read(fd, &ctx->recv_frame_hdr_buf, sizeof(ctx->recv_frame_hdr_buf));
    if (r <= 0) {
        /* TODO Check if we want better error handling. */
        return SELVA_PROTO_ECONNRESET;
    } else if (r < (ssize_t)sizeof(struct selva_proto_header)) {
        return SELVA_PROTO_EBADMSG;
    }

    frame_bsize = le16toh(ctx->recv_frame_hdr_buf.frame_bsize); /* We know it's aligned. */
    if (frame_bsize > SELVA_PROTO_FRAME_SIZE_MAX) {
        return SELVA_PROTO_EBADMSG;
    }

    const size_t frame_payload_size = frame_bsize - sizeof(struct selva_proto_header);
    if (frame_payload_size > 0) {
        /*
         * Enlarge the message buffer if necessary.
         */
        if (frame_payload_size > ctx->recv_msg_buf_size - ctx->recv_msg_buf_i) {
            const size_t new_buf_size = ctx->recv_msg_buf_size + frame_payload_size;

            selva_realloc(ctx->recv_msg_buf, new_buf_size);
            ctx->recv_msg_buf_size = new_buf_size;
        }

        r = read(fd, ctx->recv_msg_buf + ctx->recv_msg_buf_i, frame_payload_size);
        if (r <= 0) {
            /* TODO Check if we want better error handling. */
            return SELVA_PROTO_ECONNRESET;
        } else if (r != (ssize_t)frame_payload_size) {
            return SELVA_PROTO_EBADMSG;
        }

        ctx->recv_msg_buf_i += frame_payload_size;
        /* TODO Check chk */
    }

    return frame_bsize;
}

