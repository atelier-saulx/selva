#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <string.h>

#include "./modify.h"

#define CLIENT_SOCK_FILE "/tmp/selva.sock"

extern int errno;

int fd = -1;
struct sockaddr_un addr;

int SelvaModify_SendAsyncTask(int payload_size, char *payload, uint8_t retries) {
  if (fd == -1) {
    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
      fprintf(stderr, "Unable to open file descriptor for %s\n", CLIENT_SOCK_FILE);
      fd = -1;
      goto error;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIENT_SOCK_FILE, sizeof(addr.sun_path)-1);


    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      fprintf(stderr, "Error (%s) connecting to %s\n", strerror(errno), addr.sun_path);
      goto error;
    }
  }

  if (write(fd, payload, payload_size) != payload_size) {
    fprintf(stderr, "Error (%s) writing to socket\n", strerror(errno));
    goto error;
  }

  if (write(fd, "\n", 1) != 1) {
    fprintf(stderr, "Error (%s) writing to socket\n", strerror(errno));
    goto error;
  }

  return 0;

error:
  if (fd > 0) {
    close(fd);
  }

  fd = -1;

  if (retries <= 0) {
    fprintf(stderr, "Retries exceeded\n");
    exit(1);
  }

  return SelvaModify_SendAsyncTask(payload_size, payload, --retries);
}

void SelvaModify_PreparePublishPayload(char *payload_str, const char *id_str, size_t id_size, const char *field_str, size_t field_size) {
  size_t struct_size = sizeof(struct SelvaModify_AsyncTask);

  struct SelvaModify_AsyncTask publish_task;
  publish_task.type = SELVA_MODIFY_ASYNC_TASK_PUBLISH;
  memcpy(publish_task.id, id_str, 10);
  publish_task.field_name = (const char *)struct_size;
  publish_task.field_name_len = field_size;
  publish_task.value = NULL;
  publish_task.value_len = 0;

  char *ptr = payload_str;

  int32_t total_size = struct_size + field_size;
  memcpy(ptr, &total_size, sizeof(int32_t));
  ptr += sizeof(int32_t);

  memcpy(ptr, &publish_task, struct_size);
  ptr += struct_size;

  memcpy(ptr, field_str, field_size);
}

void SelvaModify_PrepareValueIndexPayload(char *payload_str, const char *id_str, size_t id_size, const char *field_str, size_t field_size, const char *value_str, size_t value_size) {
  size_t struct_size = sizeof(struct SelvaModify_AsyncTask);
  struct SelvaModify_AsyncTask publish_task;
  publish_task.type = SELVA_MODIFY_ASYNC_TASK_INDEX;
  memcpy(publish_task.id, id_str, 10);
  publish_task.field_name = (const char *)struct_size;
  publish_task.field_name_len = field_size;
  publish_task.value = (const char *)struct_size + field_size;
  publish_task.value_len = value_size;

  char *ptr = payload_str;

  int32_t total_size = struct_size + field_size + value_size;
  memcpy(ptr, &total_size, sizeof(int32_t));
  ptr += sizeof(int32_t);

  memcpy(ptr, &publish_task, struct_size);
  ptr += struct_size;

  memcpy(ptr, field_str, field_size);
  ptr += field_size;
  memcpy(ptr, value_str, value_size);
}
