#include <alloca.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "cdefs.h"
#include "endian.h"
#include "util/eztrie.h"
#include "selva_proto.h"

#define PORT 3000

struct eztrie commands;

[[nodiscard]]
static int connect_to_server(void)
{
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Could not create a socket\n");
        return -1;
    }

    (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) == -1) {
        fprintf(stderr, "Invalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "Connection failed\n");
        return -1;
    }

    return sock;
}

static void clear_crlf(char *buf)
{
    char *c = strpbrk(buf, "\r\n");

    if (c) {
        *c = '\0';
    }
}

static int get_cmd(void)
{
    char buf[80];
    struct eztrie_iterator it;
    struct eztrie_node_value *v;

    printf("> ");
    if (fgets(buf, sizeof(buf), stdin)) {
        clear_crlf(buf);

        it = eztrie_find(&commands, buf);
        v = eztrie_remove_ithead(&it);

        if (v && !strcmp(v->key, buf)) {
            return (int)v->p;
        } else {
            fprintf(stderr, "Unknown command\n");
        }
    }

    return -1;
}

void *recv_message(int fd,int *cmd, size_t *msg_size)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[4096];
    struct selva_proto_header resp_hdr;
    ssize_t r;
    size_t i = 0;

    do {
        r = recv(fd, &resp_hdr, sizeof(resp_hdr), 0);
        if (r != (ssize_t)sizeof(resp_hdr)) {
            return NULL;
        } else {
            size_t frame_bsize = le16toh(resp_hdr.frame_bsize);
            const size_t payload_size = frame_bsize - sizeof(resp_hdr);

            if (!(resp_hdr.flags & SELVA_PROTO_HDR_FREQ_RES) ||
                i + payload_size > sizeof(msg_buf)) {
                return NULL;
            }

            if (payload_size > 0) {
                r = recv(fd, msg_buf + i, payload_size, 0);
                if (r != (ssize_t)payload_size) {
                    return NULL;
                }

                i += payload_size;
            }
        }
    } while (!(resp_hdr.flags & SELVA_PROTO_HDR_FLAST));

    *cmd = resp_hdr.cmd;
    *msg_size = i;
    return msg_buf;
}

int main(int argc, char const* argv[])
{
    static int seqno = 0;
     int sock = connect_to_server();

     if (sock == -1) {
         exit(EXIT_FAILURE);
     }

     for (int cmd = get_cmd(); cmd != -2; cmd = get_cmd()) {
         _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
         struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

         if (cmd == -1) continue;

         memset(hdr, 0, sizeof(*hdr));
         hdr->cmd = cmd;
         hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
         hdr->seqno = htole32(seqno++);
         hdr->frame_bsize = htole16(sizeof(buf));
         hdr->msg_bsize = 0;

#if 0
         printf("cmd_id: %d\n", cmd);
#endif

         if (send(sock, buf, sizeof(buf), MSG_MORE) != sizeof(buf)) {
             fprintf(stderr, "Send failed\n");
             break;
         }

         int resp_cmd;
         size_t msg_size;
         uint8_t *msg = recv_message(sock, &resp_cmd, &msg_size);
         if (!msg) {
             fprintf(stderr, "Reading response failed\n");
             continue;
         }

         switch (resp_cmd) {
         case 0: /* Ping */
             if (msg_size >= sizeof(struct selva_proto_control)) {
                 struct selva_proto_control *ctrl = (struct selva_proto_control *)msg;

                 if (ctrl->type == SELVA_PROTO_STRING && msg_size >= sizeof(struct selva_proto_string)) {
                     struct selva_proto_string *s = (struct selva_proto_string *)msg;

                     if (le32toh(s->bsize) <= msg_size - sizeof(struct selva_proto_string)) {
                        printf("%.*s\n", (int)s->bsize, s->str);
                     } else {
                         fprintf(stderr, "Invalid string\n");
                     }
                 } else {
                     fprintf(stderr, "Unexpected response to \"ping\": %d\n", ctrl->type);
                 }
             } else {
                 fprintf(stderr, "Response is shorter than expected\n");
             }
             break;
         default:
             fprintf(stderr, "Unsupported command response\n");
         }
     }

     close(sock);
     return EXIT_SUCCESS;
}

__constructor static void init(void)
{
    eztrie_init(&commands);
    eztrie_insert(&commands, "ping", (void *)0);
}
