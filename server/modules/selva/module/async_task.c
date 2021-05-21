#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hiredis/hiredis.h>

#include "cdefs.h"
#include "async_task.h"
#include "hierarchy.h"
#include "subscriptions.h"
#include "queue_r.h"

#define CHANNEL_SUB_ID(prefix, sub_id) \
            char channel[sizeof(prefix) + SELVA_SUBSCRIPTION_ID_STR_LEN] = prefix; \
            Selva_SubscriptionId2str(channel + sizeof(prefix) - 1, (sub_id));

/**
 * Worker restart period. [min]
 */
#define WORKER_RESTART_PERIOD 5
/**
 * Worker hiredis max reconnection attempts.
 */
#define WORKER_MAX_RECONN 3

#if __MACH__
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC
#elif defined(CLOCK_MONOTONIC_COARSE)
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC_COARSE
#elif _POSIX_MONOTONIC_CLOCK > 0
#define WORKER_TIME_SOURCE CLOCK_MONOTONIC
#else
#define WORKER_TIME_SOURCE CLOCK_REALTIME
#endif

static uint64_t total_publishes;
static uint64_t missed_publishes;

static pthread_t thread_ids[ASYNC_TASK_HIREDIS_WORKER_COUNT] = { };
_Alignas(ASYNC_TASK_RING_BUF_BLOCK_SIZE) static char queue_mem[ASYNC_TASK_HIREDIS_WORKER_COUNT][ASYNC_TASK_RING_BUF_BLOCK_SIZE * ASYNC_TASK_RING_BUF_LENGTH];
static queue_cb_t queues[ASYNC_TASK_HIREDIS_WORKER_COUNT];

__constructor static void initialize_queues() {
    for (uint8_t i = 0; i < ASYNC_TASK_HIREDIS_WORKER_COUNT; i++) {
        queues[i] = QUEUE_INITIALIZER(queue_mem[i], ASYNC_TASK_RING_BUF_BLOCK_SIZE, sizeof(queue_mem[i]));
    }
}

static uint8_t queue_idx = 0;
static inline uint8_t next_queue_idx() {
    uint8_t idx = queue_idx;
    queue_idx = (queue_idx + 1) % ASYNC_TASK_HIREDIS_WORKER_COUNT;
    return idx;
}

static int getRedisPort(void) {
    const char *str;

    str = getenv("REDIS_PORT");
    if (!str) {
        return 0;
    }

    return strtol(str, NULL, 10);
}

static void selva_yield(void) {
    sched_yield();
    usleep(ASYNC_TASK_PEEK_INTERVAL_US);
}

#define ASYNC_TASK_LOG(fmt, ...) \
    fprintf(stderr, "%s:%d:[tid:%d]: " fmt, \
            __FILE__, __LINE__, (int)thread_idx, ##__VA_ARGS__)

void *SelvaModify_AsyncTaskWorkerMain(void *argv) {
    uint64_t thread_idx = (uint64_t)argv;
    redisContext *ctx = NULL;

    ASYNC_TASK_LOG("Started async task worker\n");

    queue_cb_t *queue = queues + thread_idx;

    int port = getRedisPort();
    if (!port) {
        ASYNC_TASK_LOG("REDIS_PORT invalid or not set\n");
        goto error;
    }

    ASYNC_TASK_LOG("Connecting to Redis master on 127.0.0.1:%d\n", port);

    ctx = redisConnect("127.0.0.1", port);
    if (ctx->err) {
        ASYNC_TASK_LOG("Error connecting to the redis instance\n");
        usleep(100000);
        goto error;
    }

    struct timespec start_time;
    struct timespec cur_time;
    int retry_cnt = 0;
    for (clock_gettime(WORKER_TIME_SOURCE, &start_time), clock_gettime(WORKER_TIME_SOURCE, &cur_time);
         (cur_time.tv_sec - start_time.tv_sec) / 60 < WORKER_RESTART_PERIOD + (long long)thread_idx;
         clock_gettime(WORKER_TIME_SOURCE, &cur_time)) {
        char *next;

        if (!queue_peek(queue, (void **)&next)) {
            selva_yield();
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
                    selva_yield();
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
            ASYNC_TASK_LOG("Reconnecting to Redis master on 127.0.0.1:%d\n", port);

            ctx = redisConnect("127.0.0.1", port);
            if (ctx->err) {
                ASYNC_TASK_LOG("Error connecting to the redis instance\n");
                usleep(20000);
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

error:
    ASYNC_TASK_LOG("Thread restarting... Ran for %ld minutes", (cur_time.tv_sec - start_time.tv_sec) / 60);
    thread_ids[thread_idx] = 0;
    redisFree(ctx);

    return NULL;
}

int SelvaModify_SendAsyncTask(const char *payload, size_t payload_len) {
    if (ASYNC_TASK_DEBUG_DROP_ALL) {
        return 0;
    }

    for (size_t i = 0; i < ASYNC_TASK_HIREDIS_WORKER_COUNT; i++) {
        if (thread_ids[i] == 0) {
            pthread_create(&thread_ids[i], NULL, SelvaModify_AsyncTaskWorkerMain, (void *)i);
        }
    }

    uint8_t first_worker_idx = next_queue_idx();
    uint8_t worker_idx = first_worker_idx;
    if (queue_isfull(&queues[worker_idx])) {
        do {
            worker_idx = next_queue_idx();
        } while(queue_isfull(&queues[worker_idx]) && worker_idx != first_worker_idx);

        if (worker_idx == first_worker_idx) {
            missed_publishes++;
            fprintf(stderr, "MISSED PUBLISH: %lli / %lli \n", (long long)missed_publishes, (long long)total_publishes);
            return 1;
        }
    }

    size_t offset = 0;
    size_t bcount = payload_len;
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

    total_publishes++;

    return 0;
}

void SelvaModify_PublishSubscriptionUpdate(const Selva_SubscriptionId sub_id) {
    const size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
    const size_t payload_len = sizeof(int32_t) + struct_len;
    int32_t total_len = payload_len;
    char payload_str[payload_len];
    char *ptr = payload_str;
    struct SelvaModify_AsyncTask publish_task = {
        .type = SELVA_MODIFY_ASYNC_TASK_SUB_UPDATE,
    };

    memcpy(publish_task.sub_update.sub_id, sub_id, SELVA_SUBSCRIPTION_ID_SIZE);
    memcpy(ptr, &total_len, sizeof(int32_t));
    ptr += sizeof(int32_t);
    memcpy(ptr, &publish_task, struct_len);

    SelvaModify_SendAsyncTask(payload_str, payload_len);
}

void SelvaModify_PublishSubscriptionTrigger(const Selva_SubscriptionId sub_id, const Selva_NodeId node_id) {
    const size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
    const size_t payload_len = sizeof(int32_t) + struct_len;
    int32_t total_len = payload_len;
    char payload_str[payload_len];
    char *ptr = payload_str;
    struct SelvaModify_AsyncTask publish_task = {
        .type = SELVA_MODIFY_ASYNC_TASK_SUB_TRIGGER,
    };

    memcpy(publish_task.sub_trigger.sub_id, sub_id, SELVA_SUBSCRIPTION_ID_SIZE);
    memcpy(publish_task.sub_trigger.node_id, node_id, SELVA_NODE_ID_SIZE);
    memcpy(ptr, &total_len, sizeof(int32_t));
    ptr += sizeof(int32_t);
    memcpy(ptr, &publish_task, struct_len);

    SelvaModify_SendAsyncTask(payload_str, payload_len);
}
