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
    fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
      fprintf(stderr, "Unable to open file descriptor for %s\n", CLIENT_SOCK_FILE);
      fd = -1;
      goto error;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CLIENT_SOCK_FILE, sizeof(addr.sun_path)-1);
    unlink(CLIENT_SOCK_FILE);


    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      fprintf(stderr, "Error (%s) connecting to %s\n", strerror(errno), addr.sun_path);
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

  if (retries <= 0) {
    fprintf(stderr, "Retries exceeded\n");
    exit(1);
  }

  return SelvaModify_SendAsyncTask(payload_size, payload, --retries);
}
