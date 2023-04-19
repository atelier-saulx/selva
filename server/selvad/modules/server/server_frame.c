/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "util/crc32c.h"
#include "util/tcp.h"
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "server.h"

#define MAX_RETRIES 3

static int send_frame(int sockfd, const void *buf, size_t len, int flags)
{
    int retry_count = 0;
    ssize_t res;

retry:
    res = tcp_send(sockfd, buf, len, flags);
    if (res < 0) {
        switch (errno) {
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
        case ENOBUFS:
            if (retry_count++ > MAX_RETRIES) {
                return SELVA_PROTO_ENOBUFS; /* Not quite exact for EAGAIN but good enough. */
            } else {
                /*
                 * The safest thing to do is a blocking sleep so this
                 * thread/process will give the kernel some time to
                 * flush its buffers.
                 */
                const struct timespec tim = {
                    .tv_sec = 0,
                    .tv_nsec = 500, /* *sleeve-shaking* */
                };

                nanosleep(&tim, NULL);
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

static void set_resp_msg_len(struct selva_server_response_out *resp, size_t bsize)
{
    struct selva_proto_header *hdr = (struct selva_proto_header *)resp->buf;

    hdr->msg_bsize = bsize;
}

static void finalize_frame(void *buf, size_t bsize, int last_frame)
{
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    hdr->flags |= last_frame ? SELVA_PROTO_HDR_FLAST : 0;
    hdr->frame_bsize = htole16(bsize);
    hdr->chk = 0;
    hdr->chk = htole32(crc32c(0, buf, bsize));
}

int server_flush_frame_buf(struct selva_server_response_out *resp, int last_frame)
{
    int err;

    if (resp->buf_i == 0) {
        if (last_frame) {
            start_resp_frame_buf(resp);
        } else {
            /*
             * Nothing to flush.
             * Usually this means that the caller is starting a stream.
             */
            return 0;
        }
    }

    finalize_frame(resp->buf, resp->buf_i, last_frame);
    err = send_frame(resp->ctx->fd, resp->buf, resp->buf_i, 0);
    resp->buf_i = 0;

    return err;
}

/**
 * Cork the underlying socket if not yet corked.
 */
static void maybe_cork(struct selva_server_response_out *resp)
{
    struct conn_ctx *ctx = resp->ctx;

    if (!ctx->corked) {
        tcp_cork(ctx->fd);
        ctx->corked = 1;
    }
}

/**
 * Uncork the underlying socket if conditions are met.
 * Uncorks the underlying socket if response or conn_ctx properties don't require
 * corked socket.
 */
static void maybe_uncork(struct selva_server_response_out *resp, enum server_send_flags flags)
{
    struct conn_ctx *ctx = resp->ctx;

    if ((flags & SERVER_SEND_MORE) == 0 && resp->cork == 0 && ctx && !ctx->batch_active) {
        tcp_uncork(ctx->fd);
        ctx->corked = 0;
    }
}

void server_cork_resp(struct selva_server_response_out *resp)
{
    resp->cork = 1;
    maybe_cork(resp);
}

void server_uncork_resp(struct selva_server_response_out *resp)
{
    if (resp->cork) {
        resp->cork = 0;
        if (resp->ctx) {
            maybe_uncork(resp, 0);
        }
    }
}


ssize_t server_send_buf(struct selva_server_response_out *restrict resp, const void *restrict buf, size_t len, enum server_send_flags flags)
{
    size_t i = 0;
    ssize_t ret = (ssize_t)len;

    if (!resp->ctx) {
        return SELVA_PROTO_ENOTCONN;
    }

    maybe_cork(resp);
    while (i < len) {
        if (resp->buf_i >= sizeof(resp->buf)) {
            int err;
            err = server_flush_frame_buf(resp, 0);
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
    maybe_uncork(resp, (ret < 0) ? flags & ~SERVER_SEND_MORE : flags);
    return ret;
}

ssize_t server_send_file(struct selva_server_response_out *resp, int fd, size_t size, enum server_send_flags flags)
{
    if (!resp->ctx) {
        return SELVA_PROTO_ENOTCONN;
    }

    maybe_cork(resp);

    /*
     * Create and send a new frame header with no payload and msg_bsize set.
     */
    server_flush_frame_buf(resp, 0);
    start_resp_frame_buf(resp);
    set_resp_msg_len(resp, size);
    server_flush_frame_buf(resp, 0);

    off_t bytes_sent = tcp_sendfile(resp->ctx->fd, fd, &(off_t){0}, size);
    if (bytes_sent != (off_t)size) {
        /*
         * Some of the errors are not SELVA_PROTO but ¯\_(ツ)_/¯
         */
        switch (errno) {
        case EBADF:
            bytes_sent = SELVA_PROTO_EBADF;
            break;
        case EFAULT:
        case EINVAL:
            bytes_sent = SELVA_EINVAL;
            break;
        case EIO:
            bytes_sent = SELVA_EIO;
            break;
        case ENOMEM:
        case EOVERFLOW:
            bytes_sent = SELVA_PROTO_ENOBUFS;
            break;
        case ESPIPE:
            bytes_sent = SELVA_PROTO_EPIPE;
            break;
        default:
            bytes_sent = SELVA_EGENERAL;
            break;
        }
    }

    maybe_uncork(resp, (bytes_sent < 0) ? flags & ~SERVER_SEND_MORE : flags);
    return bytes_sent;
}

int selva_start_stream(struct selva_server_response_out *resp, struct selva_server_response_out **stream_resp_out)
{
    struct selva_server_response_out *stream_resp;

    if (!resp->ctx) {
        return SELVA_PROTO_ENOTCONN;
    }

    if (resp->frame_flags & SELVA_PROTO_HDR_STREAM) {
        /* Stream already started. */
        return SELVA_PROTO_EALREADY;
    }

    stream_resp = alloc_stream_resp(resp->ctx);
    if (!stream_resp) {
        return SELVA_PROTO_ENOBUFS;
    }

    server_flush_frame_buf(resp, 0);
    resp->frame_flags |= SELVA_PROTO_HDR_STREAM;
    memcpy(stream_resp, resp, sizeof(*stream_resp));
    stream_resp->cork = 0; /* Streams should not be corked at response level. */

    *stream_resp_out = stream_resp;
    return 0;
}

void selva_cancel_stream(struct selva_server_response_out *resp, struct selva_server_response_out *stream_resp)
{
    resp->frame_flags &= ~SELVA_PROTO_HDR_STREAM;
    free_stream_resp(stream_resp);
}

ssize_t server_recv_frame(struct conn_ctx *ctx)
{
    int fd = ctx->fd;
    ssize_t r;

    /* TODO We might want to do this in a single read and add more buffering to reduce syscall overhead. */
    r = tcp_read(fd, &ctx->recv_frame_hdr_buf, sizeof(ctx->recv_frame_hdr_buf));
    if (r <= 0) {
        /* Drop the connection immediately. */
        return SELVA_PROTO_ECONNRESET;
    } else if (r != (ssize_t)sizeof(struct selva_proto_header)) {
        return SELVA_PROTO_EBADMSG;
    }

    const ssize_t frame_bsize = le16toh(ctx->recv_frame_hdr_buf.frame_bsize); /* We know it's aligned. */
    const size_t frame_payload_size = frame_bsize - sizeof(struct selva_proto_header);

    if (frame_payload_size > SELVA_PROTO_FRAME_SIZE_MAX) {
        return SELVA_PROTO_EBADMSG;
    } else if (frame_payload_size > 0) {
        /*
         * Resize the message buffer if necessary.
         */
        if (frame_payload_size > ctx->recv_msg_buf_size - ctx->recv_msg_buf_i) {
            realloc_ctx_msg_buf(ctx, ctx->recv_msg_buf_size + frame_payload_size);
        }

        r = tcp_read(fd, ctx->recv_msg_buf + ctx->recv_msg_buf_i, frame_payload_size);
        if (r <= 0) {
            /*
             * Just drop the connection immediately to keep the server side
             * connection handling simple. The client can handle connection
             * issues better.
             */
            return SELVA_PROTO_ECONNRESET;
        } else if (r != (ssize_t)frame_payload_size) {
            return SELVA_PROTO_EBADMSG;
        }

        ctx->recv_msg_buf_i += frame_payload_size;
    }

    /*
     * Verify the frame checksum.
     */
    if (!selva_proto_verify_frame_chk(&ctx->recv_frame_hdr_buf,
                                      ctx->recv_msg_buf + ctx->recv_msg_buf_i - frame_payload_size,
                                      frame_payload_size)) {
        /* Discard the frame */
        ctx->recv_msg_buf_i -= frame_payload_size;
        return SELVA_PROTO_EBADMSG;
    }

    return frame_bsize;
}
