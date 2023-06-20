/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "util/svector.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "server.h"

#define NR_CHANNELS (sizeof(pubsub_ch_mask_t) * CHAR_BIT)

struct channel {
    SVector subscribers;
};

struct channel channels[NR_CHANNELS];

static void pubsub_del_from_channel(struct channel *channel, struct conn_ctx *ctx, void (*cb)(struct selva_server_response_out *stream_resp))
{
    SVector *vec = &channel->subscribers;
    size_t len = SVector_Size(vec);

    /*
     * Foreach resp in &channel->subscribers in case the client subscribed to
     * the same channel multiple times, even if it shouldn't be possible.
     * An added bonus is that we'll always get to delete ended streams, which
     * hopefully are still valid pointers until now.
     */
    for (size_t i = 0; i < len;) {
        struct selva_server_response_out *stream_resp;

        stream_resp = SVector_GetIndex(vec, i);
        assert(stream_resp);

        if (!stream_resp->ctx) {
            /* This stream was already ended. */
            SVector_RemoveIndex(vec, i);
            len--;
        } else if (stream_resp->ctx == ctx) {
            SVector_RemoveIndex(vec, i);
            cb(stream_resp);
            len--;
        } else {
            i++;
        }
    }
}

static void pubsub_del_all_streams(struct conn_ctx *ctx, void (*cb)(struct selva_server_response_out *stream_resp))
{
    for (size_t ch_id = 0; ch_id < NR_CHANNELS; ch_id++) {
        if (ctx->pubsub_ch_mask & (1 << ch_id)) {
            struct channel *channel = &channels[ch_id];

            pubsub_del_from_channel(channel, ctx, cb);
        }
    }

    ctx->pubsub_ch_mask = 0;
}

void pubsub_teardown(struct conn_ctx *ctx)
{
    /*
     * free_stream_resp() is practically a simplified selva_send_end()
     * (or pubsub_end(()) that assumes the connection is already defunct and
     * thus flushing and any other jiggle is pointless.
     */
    pubsub_del_all_streams(ctx, free_stream_resp);
}

/**
 * End a subscription stream gracefully.
 */
static void pubsub_end(struct selva_server_response_out *stream_resp)
{
    (void)selva_send_end(stream_resp);
}

void pubsub_unsubscribe_all(struct conn_ctx *ctx)
{
    pubsub_del_all_streams(ctx, pubsub_end);
}

int pubsub_unsubscribe(struct conn_ctx *ctx, unsigned ch_id)
{
    pubsub_ch_mask_t mask;

    if (ch_id >= NR_CHANNELS) {
        return SELVA_EINVAL;
    }

    mask = (1 << ch_id);

    if (!(ctx->pubsub_ch_mask & mask)) {
        return SELVA_ENOENT;
    }

    pubsub_del_from_channel(&channels[ch_id], ctx, pubsub_end);
    ctx->pubsub_ch_mask ^= mask;
    return 0;
}

static void publish(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    unsigned ch_id;
    const char *message_str;
    size_t message_len;
    int argc;
    struct SVectorIterator it;
    struct selva_server_response_out *stream_resp;

    argc = selva_proto_scanf(NULL, buf, len, "%u, %.*s", &ch_id, &message_len, &message_str);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    if (ch_id >= NR_CHANNELS) {
        selva_send_errorf(resp, SELVA_EINVAL, "Invalid channel id");
        return;
    }

    SVector_ForeachBegin(&it, &channels[ch_id].subscribers);
    while ((stream_resp = SVector_Foreach(&it))) {
        selva_send_str(stream_resp, message_str, message_len);
        (void)selva_send_flush(stream_resp);
        /*
         * We ignore errors for now and let on_close handler take care of the
         * cleanup.
         */
    }

    selva_send_ll(resp, 1);
}

static void subscribe(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    unsigned ch_id;
    int argc, err;
    struct selva_server_response_out *stream_resp;

    argc = selva_proto_scanf(NULL, buf, len, "%u", &ch_id);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    if (!resp->ctx) {
        selva_send_errorf(resp, SELVA_EINVAL, "This command requires ctx");
        return;
    }

    if (ch_id >= NR_CHANNELS) {
        selva_send_errorf(resp, SELVA_EINVAL, "Invalid channel id");
        return;
    } else if (resp->ctx->pubsub_ch_mask & (1 << ch_id)) {
        selva_send_errorf(resp, SELVA_EEXIST, "Already subscribed");
    }

    err = selva_start_stream(resp, &stream_resp);
    if (err) {
        selva_send_errorf(resp, err, "Failed to create a stream");
        return;
    }

    SVector_Insert(&channels[ch_id].subscribers, stream_resp);
    stream_resp->ctx->pubsub_ch_mask |= (1 << ch_id);
}

static void unsubscribe(struct selva_server_response_out *resp, const void *buf, size_t len)
{
    unsigned ch_id;
    int argc, err;

    argc = selva_proto_scanf(NULL, buf, len, "%u", &ch_id);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    err = pubsub_unsubscribe(resp->ctx, ch_id);
    if (err) {
        selva_send_errorf(resp, err, "Failed to unsubscribe");
        return;
    }

    selva_send_ll(resp, 1);
}

void pubsub_init(void)
{
    for (size_t i = 0; i < num_elem(channels); i++) {
        SVector_Init(&channels[i].subscribers, 0, NULL);
    }

    SELVA_MK_COMMAND(CMD_ID_PUBLISH, SELVA_CMD_MODE_PURE, publish);
    SELVA_MK_COMMAND(CMD_ID_SUBSCRIBE, SELVA_CMD_MODE_PURE, subscribe);
    SELVA_MK_COMMAND(CMD_ID_UNSUBSCRIBE, SELVA_CMD_MODE_PURE, unsubscribe);
}
