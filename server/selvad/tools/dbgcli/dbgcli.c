#include <arpa/inet.h>
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

         memset(hdr, 0, sizeof(*hdr));
         hdr->cmd = cmd;
         hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
         hdr->seqno = htole32(seqno++);
         hdr->frame_bsize = htole16(sizeof(buf));
         hdr->msg_bsize = 0;

         if (cmd == -1) continue;

         printf("cmd_id: %d\n", cmd);

         if (send(sock, buf, sizeof(buf), MSG_MORE) != sizeof(buf)) {
             fprintf(stderr, "Send failed\n");
             break;
         }

         _Alignas(struct selva_proto_header) char buf1[1024];
         ssize_t r;

         r = recv(sock, buf1, sizeof(buf1) - 1, 0);
         if (r >= (ssize_t)sizeof(struct selva_proto_header)) {
             struct selva_proto_header *resp = (struct selva_proto_header *)buf1;

             if (resp->flags & SELVA_PROTO_HDR_FREQ_RES) {
                switch (resp->cmd) {
                case 0: /* Ping */
                    if (r >= (ssize_t)sizeof(struct selva_proto_control)) {
                        struct selva_proto_control ctrl;

                        memcpy(&ctrl, resp + sizeof(struct selva_proto_header), sizeof(ctrl));
                        if (ctrl.type == SELVA_PROTO_STRING) {
                            struct selva_proto_string *s = (struct selva_proto_string *)(resp + sizeof(struct selva_proto_header));

                            printf("%.*s\n", (int)s->bsize, s->str);
                        } /* TODO else */
                    } /* TODO else */
                    break;
                default:
                    fprintf(stderr, "Unsupported command response\n");
                }
             } else {
                 fprintf(stderr, "Unexpected message received\n");
             }
         } else {
            fprintf(stderr, "Received message is too small (%d bytes)\n", (int)r);
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
