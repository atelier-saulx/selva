/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"

/* TODO Nothing to see here yet */

#define WORKER_COUNT 1

#if __MACH__
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC
#elif defined(CLOCK_MONOTONIC_COARSE)
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC_COARSE
#elif _POSIX_MONOTONIC_CLOCK > 0
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC
#else
#define WORKER_TIME_SOURCE CLOCK_REALTIME
#endif

static pthread_t thread_ids[WORKER_COUNT] = { };
_Alignas(uint64_t) static char queue_mem[WORKER_COUNT][SERVER_RING_BUF_LEN * SERVER_RING_BUF_BLOCK_SIZE];
static queue_cb_t queues[WORKER_COUNT];

int server_assign_worker(struct conn_ctx *ctx)
{
    ctx->worker_id = thread_ids[ctx->fd % WORKER_COUNT];
}

static void worker_nsleep(long nsec)
{
    const struct timespec tim = {
        .tv_sec = 0,
        .tv_nsec = nsec,
    };

    nanosleep(&tim, NULL);
}

static void worker_yield(void)
{
    sched_yield();
    async_task_nsleep(ASYNC_TASK_PEEK_INTERVAL_NS);
}

static void *server_worker_main(void *argv)
{
    uint64_t thread_id = (uint64_t)argv;
    redisContext *ctx = NULL;
    queue_cb_t *queue = queues + thread_id;

    SELVA_LOG(SELVA_LOGL_INFO, "Started server IO worker: %d", thread_id);

    while (true) {
        char *next;

        if (!queue_peek(queue, (void **)&next)) {
            async_task_yield();
            continue;
        }

        const int32_t size = *((int32_t *)next);

        if (size <= 0) {
            queue_skip(queue, 1);
            continue;
        }

        _Alignas(struct SelvaModify_AsyncTask) char read_buffer[size];
        char *read_ptr = read_buffer;
        size_t remaining = size;
        size_t block_remaining = ASYNC_TASK_RING_BUF_BLOCK_SIZE - sizeof(int32_t);

        next += sizeof(int32_t);
        do {
            const size_t to_read = min(block_remaining, remaining);

            memcpy(read_ptr, next, to_read);
            queue_skip(queue, 1);

            block_remaining = ASYNC_TASK_RING_BUF_BLOCK_SIZE;
            remaining -= to_read;
            next += to_read;
            read_ptr += to_read;

            if (remaining > 0) {
                if (!queue_peek(queue, (void **)&next)) {
                    async_task_yield();
                    continue;
                }
            }
        } while (remaining > 0);

        struct SelvaModify_AsyncTask *task = (struct SelvaModify_AsyncTask *)read_buffer;

#if 0
        fprintf(stderr, "New task received. type: %d size: %d bytes\n", (int)task->type, (int)size);
#endif

#define RETRY \
        redisFree(ctx); \
        ctx = NULL; \
        retry_cnt++; \
        goto retry
retry:
        if (retry_cnt > WORKER_MAX_RECONN) {
            goto error;
        }
        if (!ctx) {
            ASYNC_TASK_LOG("Reconnecting to Redis master on %s:%d\n", redis_addr, port);

            ctx = redisConnect(redis_addr, port);
            if (ctx->err) {
                ASYNC_TASK_LOG("Error connecting to the redis instance\n");
                async_task_nsleep(20000000L);
                RETRY;
            }
        }

        if (task->type == SELVA_MODIFY_ASYNC_TASK_SUB_UPDATE) {
            CHANNEL_SUB_ID("___selva_subscription_update:", task->sub_update.sub_id);
            redisReply *reply = NULL;

#if 0
            fprintf(stderr, "Redis publish subscription update: \"%s\"\n", channel);
#endif
            reply = redisCommand(ctx, "PUBLISH %s %s", channel, "");
            if (!reply) {
                ASYNC_TASK_LOG("No reply received: %s\n", ctx->errstr);
                RETRY;
            }
            if (reply->type == REDIS_REPLY_ERROR) {
                ASYNC_TASK_LOG("Error occurred in publish %s\n", ctx->errstr);
                RETRY;
            }

            freeReplyObject(reply);
        } else if (task->type == SELVA_MODIFY_ASYNC_TASK_SUB_TRIGGER) {
            CHANNEL_SUB_ID("___selva_subscription_trigger:", task->sub_trigger.sub_id);
            redisReply *reply = NULL;

#if 0
            fprintf(stderr, "Redis publish subscription trigger: \"%s\"\n", channel);
#endif
            reply = redisCommand(ctx, "PUBLISH %s %b", channel, task->sub_trigger.node_id, SELVA_NODE_ID_SIZE);
            if (!reply) {
                ASYNC_TASK_LOG("No reply received: %s\n", ctx->errstr);
                RETRY;
            }
            if (reply->type == REDIS_REPLY_ERROR) {
                ASYNC_TASK_LOG("Error occurred in publish %s\n", ctx->errstr);
                RETRY;
            }

            retry_cnt = 0;
            freeReplyObject(reply);
        } else {
            ASYNC_TASK_LOG("Unsupported task type %d\n", task->type);
        }
#undef RETRY
    }

    return NULL;
}

void server_dispatch2worker(struct conn_ctx *restrict ctx, const char *restrict payload, size_t payload_len)
{
    pthread_t worker_id = ctx->worker_id;
    size_t offset = 0;
    size_t bcount = payload_len;

    while (queue_isfull(&queues[worker_idx])) {
        worker_nsleep(1000);
    }

    do {
        char *ptr;

        ptr = queue_alloc_get(&queues[worker_idx]);
        if (ptr) {
            const size_t to_write = min(bcount, (size_t)ASYNC_TASK_RING_BUF_BLOCK_SIZE);

            memcpy(ptr, payload + offset, to_write);
            queue_alloc_commit(&queues[worker_idx]);
            offset += to_write;
            bcount -= to_write;
        }
    } while (bcount > 0);
}

void server_start_workers(void)
{
    for (uint8_t i = 0; i < num_elem(queues); i++) {
        queues[i] = QUEUE_INITIALIZER(queue_mem[i], SERVER_RING_BUF_BLOCK_SIZE, sizeof(queue_mem[i]));
    }

    for (size_t i = 0; i < num_elem(thread_ids); i++) {
        pthread_attr_t attr;
        pthread_t tid;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (!pthread_create(&tid, &attr, server_worker_main, (void *)i)) {
            thread_ids[i] = tid;
        } else {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to create worked_id: %zu", i);
            exit(EXIT_FAILURE);
        }
    }
}
