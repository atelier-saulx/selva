#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <hiredis/hiredis.h>

#include "./modify.h"

#define RING_BUFFER_BLOCK_SIZE 128
#define RING_BUFFER_LENGTH 16000

pthread_t thread_id = NULL;

void *SelvaModify_AsyncTaskWorkerMain(void *argv) {
  // TODO: proper args, env?
  redisContext *ctx = redisConnect("127.0.0.1", 6379);
  redisReply *reply = NULL;

  if (ctx->err) {
    fprintf(stderr, "Error connecting to the redis instance\n");
    goto error;
  }

  for(;;) {
    reply = redisCommand(ctx, "PUBLISH %b %b", "test_channel", (size_t) 12, "hello world!", (size_t) 12);
    freeReplyObject(reply);
    reply = NULL;

    sleep(1);
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

  // TODO: put in the ring buffer pls
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
