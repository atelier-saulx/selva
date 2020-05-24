#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <string.h>

#include "./modify.h"

#define CLIENT_SOCK_FILE "/tmp/selva.sock"

int fd = -1;
struct sockaddr_un addr;

int SelvaModify_SendAsyncTask(int payload_size, char *payload, uint8_t retries) {
  if (fd == -1) {
    fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
      fprintf(stderr, "Unable to open file descriptor for %s\n", CLIENT_SOCK_FILE);
      fd = -1;
      goto error;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CLIENT_SOCK_FILE);
    unlink(CLIENT_SOCK_FILE);


    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      fprintf(stderr, "Error connecting\n");
      goto error;
    }
  }

  char buf[1024];
  sprintf(buf, "%s\n", payload);
  if (write(fd, buf, 1 + payload_size + 1) != 0) {
    fprintf(stderr, "Error writing to socket\n");
    goto error;
  }

  return 0;

error:
  if (fd > 0) {
    close(fd);
  }

  fd = -1;

  fprintf(stderr, "Error opening unix domain socket\n");
  if (retries <= 0) {
    fprintf(stderr, "Retries exceeded\n");
    exit(1);
  }

  return SelvaModify_SendAsyncTask(payload_size, payload, --retries);
}
