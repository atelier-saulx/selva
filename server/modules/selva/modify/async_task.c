#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include <hiredis/hiredis.h>

#include "cdefs.h"
#include "async_task.h"
#include "hierarchy.h"
#include "queue_r.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif

static uint64_t total_publishes;
static uint64_t missed_publishes;

static pthread_t thread_ids[ASYNC_TASK_HIREDIS_WORKER_COUNT] = { };

static char queue_mem[ASYNC_TASK_HIREDIS_WORKER_COUNT][ASYNC_TASK_RING_BUF_BLOCK_SIZE * ASYNC_TASK_RING_BUF_LENGTH];
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
    char *str;

    str = getenv("REDIS_PORT");
    if (!str) {
        return 0;
    }

    return strtol(str, NULL, 10);
}

void *SelvaModify_AsyncTaskWorkerMain(void *argv) {
    uint64_t thread_idx = (uint64_t)argv;
    redisContext *ctx = NULL;

    printf("Started worker number %i\n", (int)thread_idx);

    queue_cb_t *queue = queues + thread_idx;

    int port = getRedisPort();
    if (!port) {
        fprintf(stderr, "REDIS_PORT invalid or not set\n");
        goto error;
    }
    fprintf(stderr, "Connecting to Redis master on 127.0.0.1:%d\n", port);

    ctx = redisConnect("127.0.0.1", port);
    if (ctx->err) {
        fprintf(stderr, "Error connecting to the redis instance\n");
        goto error;
    }

    for (;;) {
        char *next;

        if (!queue_peek(queue, (void **)&next)) {
            usleep(ASYNC_TASK_PEEK_INTERVAL_US);
            continue;
        }

        const int32_t size = *((int32_t *)next);

        if (size <= 0) {
            queue_skip(queue, 1);
            continue;
        }

        char read_buffer[size];
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
                    usleep(ASYNC_TASK_PEEK_INTERVAL_US);
                    continue;
                }
            }
        } while (remaining > 0);

        struct SelvaModify_AsyncTask *task = (struct SelvaModify_AsyncTask *)read_buffer;
        task->field_name = (const char *)(read_buffer + sizeof(struct SelvaModify_AsyncTask));
        const char prefix[] = "___selva_events:";

#if 0
        fprintf(stderr, "New task received. type: %d size: %d bytes\n", (int)task->type, (int)size);
#endif

        if (task->type == SELVA_MODIFY_ASYNC_TASK_UPDATE) {
            char channel[sizeof(prefix) + SELVA_NODE_ID_SIZE + 1 + task->field_name_len];
            redisReply *reply = NULL;

            snprintf(channel, sizeof(channel), "%s%.*s.%.*s",
                     prefix,
                     (int)SELVA_NODE_ID_SIZE, task->id,
                     (int)task->field_name_len, task->field_name);

#if 0
            fprintf(stderr, "Redis publish \"%s\": \"update\"\n", channel);
#endif
            reply = redisCommand(ctx, "PUBLISH %s update", channel);
            if (reply == NULL) {
                fprintf(stderr, "Error occurred in publish %s\n", ctx->errstr);
            }

            freeReplyObject(reply);
        } else if (task->type == SELVA_MODIFY_ASYNC_TASK_CREATED) {
            char channel[sizeof(prefix) + SELVA_NODE_ID_SIZE];
            redisReply *reply = NULL;

            snprintf(channel, sizeof(channel), "%s%.*s",
                     prefix,
                     (int)SELVA_NODE_ID_SIZE, task->id);

#if 0
            fprintf(stderr, "Redis publish \"%s\": \"created\"\n", channel);
#endif
            reply = redisCommand(ctx, "PUBLISH %s created", channel);
            if (reply == NULL) {
                fprintf(stderr, "Error occurred in publish %s\n", ctx->errstr);
            }

            freeReplyObject(reply);
        } else if (task->type == SELVA_MODIFY_ASYNC_TASK_DELETED) {
            char channel[sizeof(prefix) + SELVA_NODE_ID_SIZE];
            redisReply *reply = NULL;

            snprintf(channel, sizeof(channel), "%s%.*s",
                     prefix,
                     (int)SELVA_NODE_ID_SIZE, task->id);

            const char *msg = task->field_name_len == 0 ? "" : task->field_name;

#if 0
            fprintf(stderr, "Redis publish \"%s\": \"delete:%.*s\" (%zu)\n",
                    channel,
                    (int)task->field_name_len, msg,
                    task->field_name_len);
#endif
            reply = redisCommand(ctx, "PUBLISH %s delete:%b", channel, msg, (size_t)task->field_name_len);
            if (reply == NULL) {
                fprintf(stderr, "Error occurred in publish %s\n", ctx->errstr);
            }

            freeReplyObject(reply);
        } else {
            fprintf(stderr, "Unsupported task type %d\n", task->type);
        }
    }

error:
    thread_ids[thread_idx] = 0;
    redisFree(ctx);

    return NULL;
}

int SelvaModify_SendAsyncTask(const char *payload, size_t payload_len) {
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
            printf("MISSED PUBLISH: %lli / %lli \n", (long long)missed_publishes, (long long)total_publishes);
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

void SelvaModify_PublishCreated(const char *id_str) {
    /* TODO Optimize by writing directly to payload_str */
    const size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
    const size_t payload_len = sizeof(int32_t) + struct_len;
    char payload_str[payload_len];
    struct SelvaModify_AsyncTask publish_task = {
        .type = SELVA_MODIFY_ASYNC_TASK_CREATED,
        .field_name = NULL,
        .field_name_len = 0,
    };
    strncpy(publish_task.id, id_str, SELVA_NODE_ID_SIZE);

    char *ptr = payload_str;

    int32_t total_len = struct_len;
    memcpy(ptr, &total_len, sizeof(int32_t));
    ptr += sizeof(int32_t);

    memcpy(ptr, &publish_task, struct_len);

    SelvaModify_SendAsyncTask(payload_str, payload_len);
}

void SelvaModify_PublishDeleted(const char *id_str, const char *fields) {
    /* TODO Optimize by writing directly to payload_str */
    const size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
    const size_t fields_len = fields == NULL ? 0 : strlen(fields);
    const size_t payload_len = sizeof(int32_t) + struct_len + fields_len;
    char payload_str[payload_len];
    struct SelvaModify_AsyncTask publish_task = {
        .type = SELVA_MODIFY_ASYNC_TASK_DELETED,
        .field_name = NULL,
        .field_name_len = fields_len,
    };
    strncpy(publish_task.id, id_str, SELVA_NODE_ID_SIZE);

    char *ptr = payload_str;

    int32_t total_len = struct_len + fields_len;
    memcpy(ptr, &total_len, sizeof(int32_t));
    ptr += sizeof(int32_t);

    memcpy(ptr, &publish_task, struct_len);
    ptr += struct_len;

    memcpy(ptr, fields, fields_len);

    SelvaModify_SendAsyncTask(payload_str, payload_len);
}

void SelvaModify_PublishUpdate(const char *id_str, const char *field_str, size_t field_len) {
    /* TODO Optimize by writing directly to payload_str */
    const size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
    const size_t payload_len = sizeof(int32_t) + struct_len + field_len;
    char payload_str[payload_len];
    struct SelvaModify_AsyncTask publish_task = {
        .type = SELVA_MODIFY_ASYNC_TASK_UPDATE,
        .field_name = NULL,
        .field_name_len = field_len,
    };
    strncpy(publish_task.id, id_str, SELVA_NODE_ID_SIZE);

    char *ptr = payload_str;

    int32_t total_len = struct_len + field_len;
    memcpy(ptr, &total_len, sizeof(int32_t));
    ptr += sizeof(int32_t);

    memcpy(ptr, &publish_task, struct_len);
    ptr += struct_len;

    memcpy(ptr, field_str, field_len);

    SelvaModify_SendAsyncTask(payload_str, payload_len);
}
