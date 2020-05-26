#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <hiredis/hiredis.h>

#include "./modify.h"
#include "./queue_r.h"

#define RING_BUFFER_BLOCK_SIZE 128
#define RING_BUFFER_LENGTH 16000

static inline int min(int a, int b) {
  if (a > b) {
    return b;
  }

  return a;
}

pthread_t thread_id = NULL;
char queue_mem[RING_BUFFER_BLOCK_SIZE * RING_BUFFER_LENGTH];
queue_cb_t queue = QUEUE_INITIALIZER(queue_mem, RING_BUFFER_BLOCK_SIZE, RING_BUFFER_LENGTH);

void *SelvaModify_AsyncTaskWorkerMain(void *argv) {
  // TODO: proper args, env?
  redisContext *ctx = redisConnect("127.0.0.1", 6379);
  redisReply *reply = NULL;

  if (ctx->err) {
    fprintf(stderr, "Error connecting to the redis instance\n");
    goto error;
  }

  for (;;) {
    char *next;
    int has_queue = queue_peek(&queue, (void **)&next);
    if (!has_queue) {
      usleep(100);
      continue;
    }

    int32_t size = *((int32_t *)next);
    next += sizeof(int32_t);
    char read_buffer[size];
    char *read_ptr = read_buffer;
    int32_t remaining = size;
    int32_t block_remaining = RING_BUFFER_BLOCK_SIZE - sizeof(int32_t);
    while (remaining > 0) {
      int has_queue = queue_peek(&queue, (void **)&next);
      if (!has_queue) {
        usleep(100);
        continue;
      }

      memcpy(read_ptr, next + (RING_BUFFER_BLOCK_SIZE - block_remaining), min(block_remaining, remaining));
      queue_skip(&queue, 1);
      remaining -= block_remaining;
      block_remaining = RING_BUFFER_BLOCK_SIZE;
    }


    struct SelvaModify_AsyncTask *task = (struct SelvaModify_AsyncTask *) read_buffer;
    task->field_name = (const char *)(read_buffer + sizeof(struct SelvaModify_AsyncTask));
    task->value = (const char *)(read_buffer + sizeof(struct SelvaModify_AsyncTask) + task->field_name_len);
    if (task->type == SELVA_MODIFY_ASYNC_TASK_PUBLISH) {
      char channel[11 + task->field_name_len];
      memcpy(channel, task->id, 10);
      memcpy(channel + 10, ".", 1);
      memcpy(channel + 11, task->field_name, task->field_name_len);

      reply = redisCommand(ctx, "PUBLISH %b %b", channel, (size_t) (11 + task->field_name_len), "update", (size_t) 6);

      freeReplyObject(reply);
      reply = NULL;
    } else if (task->type == SELVA_MODIFY_ASYNC_TASK_INDEX) {
      printf("TODO: index field %s with %s\n", task->field_name, task->value);
    }
  }

error:
  thread_id = NULL;
  if (reply != NULL) {
    freeReplyObject(reply);
  }

  redisFree(ctx);

  return NULL;
}

int SelvaModify_SendAsyncTask(int payload_len, char *payload) {
  if (thread_id == NULL) {
    pthread_create(&thread_id, NULL, SelvaModify_AsyncTaskWorkerMain, NULL);
  }

  for (unsigned int i = 0; i < payload_len; i += RING_BUFFER_BLOCK_SIZE) {
    void *ptr;
    while (!(ptr = queue_alloc_get(&queue)));
    memcpy(ptr, payload, payload_len);
    queue_alloc_commit(&queue);
  }

  return 0;
}

void SelvaModify_PreparePublishPayload(char *payload_str, const char *id_str, size_t id_len, const char *field_str, size_t field_len) {
  size_t struct_len = sizeof(struct SelvaModify_AsyncTask);

  struct SelvaModify_AsyncTask publish_task;
  publish_task.type = SELVA_MODIFY_ASYNC_TASK_PUBLISH;
  memcpy(publish_task.id, id_str, 10);
  publish_task.field_name = (const char *)struct_len;
  publish_task.field_name_len = field_len;
  publish_task.value = NULL;
  publish_task.value_len = 0;

  char *ptr = payload_str;

  int32_t total_len = struct_len + field_len;
  memcpy(ptr, &total_len, sizeof(int32_t));
  ptr += sizeof(int32_t);

  memcpy(ptr, &publish_task, struct_len);
  ptr += struct_len;

  memcpy(ptr, field_str, field_len);
}

void SelvaModify_PrepareValueIndexPayload(char *payload_str, const char *id_str, size_t id_len, const char *field_str, size_t field_len, const char *value_str, size_t value_len) {
  size_t struct_len = sizeof(struct SelvaModify_AsyncTask);
  struct SelvaModify_AsyncTask publish_task;
  publish_task.type = SELVA_MODIFY_ASYNC_TASK_INDEX;
  memcpy(publish_task.id, id_str, 10);
  publish_task.field_name = (const char *)struct_len;
  publish_task.field_name_len = field_len;
  publish_task.value = (const char *)struct_len + field_len;
  publish_task.value_len = value_len;

  char *ptr = payload_str;

  int32_t total_len = struct_len + field_len + value_len;
  memcpy(ptr, &total_len, sizeof(int32_t));
  ptr += sizeof(int32_t);

  memcpy(ptr, &publish_task, struct_len);
  ptr += struct_len;

  memcpy(ptr, field_str, field_len);
  ptr += field_len;
  memcpy(ptr, value_str, value_len);
}
