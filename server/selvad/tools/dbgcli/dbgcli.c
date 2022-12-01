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
#include "selva_error.h"
#include "selva_proto.h"
#include "util/eztrie.h"

#define PORT 3000

typedef int (*cmd_fn)(int);

enum selva_command_num {
    SELVA_CMD_PING = 0,
    SELVA_CMD_ECHO = 1,
    SELVA_CMD_LSCMD = 2,
    SELVA_CMD_LSLANG = 3,
    SELVA_CMD_RESOLVE_NODEID = 16,
};

struct eztrie commands;
static int seqno = 0;

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

static cmd_fn get_cmd(void)
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
            return v->p;
        } else {
            fprintf(stderr, "Unknown command\n");
        }
    }

    return NULL;
}

void *recv_message(int fd,int *cmd, size_t *msg_size)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[1048576];
    struct selva_proto_header resp_hdr;
    ssize_t r;
    size_t i = 0;

    do {
        r = recv(fd, &resp_hdr, sizeof(resp_hdr), 0);
        if (r != (ssize_t)sizeof(resp_hdr)) {
            fprintf(stderr, "recv() returned %d\n", (int)r);
            return NULL;
        } else {
            size_t frame_bsize = le16toh(resp_hdr.frame_bsize);
            const size_t payload_size = frame_bsize - sizeof(resp_hdr);

            if (!(resp_hdr.flags & SELVA_PROTO_HDR_FREQ_RES)) {
                fprintf(stderr, "Invalid response: response bit not set\n");
                return NULL;
            } else if (i + payload_size > sizeof(msg_buf)) {
                fprintf(stderr, "Buffer overflow\n");
                return NULL;
            }

            if (payload_size > 0) {
                r = recv(fd, msg_buf + i, payload_size, 0);
                if (r != (ssize_t)payload_size) {
                    fprintf(stderr, "recv() returned %d\n", (int)r);
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

static int quit(int sock __unused)
{
    return 0;
}

static int ping(int sock)
{
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = SELVA_CMD_PING;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno++);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;

    if (send(sock, buf, sizeof(buf), 0) != sizeof(buf)) {
        fprintf(stderr, "Send failed\n");
        return -1;
    }

    return 0;
}

static int echo(int sock)
{
    const char data[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
    const struct selva_proto_string str_hdr = {
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(sizeof(data)),
    };
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header) + sizeof(str_hdr) + sizeof(data)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;
    const int seq = htole32(seqno++);
    const int n = 100;

    memcpy(buf + sizeof(*hdr), &str_hdr, sizeof(str_hdr));
    memcpy(buf + sizeof(*hdr) + sizeof(str_hdr), data, sizeof(data));

    for (int i = 0; i < n; i++) {
        memset(hdr, 0, sizeof(*hdr));
        hdr->cmd = SELVA_CMD_ECHO;
        hdr->flags = (i == 0) ? SELVA_PROTO_HDR_FFIRST : (i == n - 1) ? SELVA_PROTO_HDR_FLAST : 0;
        hdr->seqno = seq;
        hdr->frame_bsize = htole16(sizeof(buf));
        hdr->msg_bsize = 0;

        if (send(sock, buf, sizeof(buf), i < n - 1 ? MSG_MORE: 0) != sizeof(buf)) {
            fprintf(stderr, "Send %d/%d failed\n", i, n);
        }
    }

    return 0;
}

static int selva_resolve(int sock)
{
    struct selva_proto_header *hdr;
    struct selva_proto_string str_hdr = {
        .type = SELVA_PROTO_STRING,
        .flags = 0,
    };
    const char alias[] = "myalias";
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header) + 2 * sizeof(struct selva_proto_string) + (sizeof(alias) - 1)];
    char *p = buf + sizeof(*hdr);

    memset(buf, 0, sizeof(buf));

    hdr = (struct selva_proto_header *)buf;
    hdr->cmd = SELVA_CMD_RESOLVE_NODEID;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno++);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0; /* TODO Can we actually ever utilize this nicely? */

    /*
     * 0: subId
     */
    str_hdr.bsize = htole32(0);
    p = memcpy(p, &str_hdr, sizeof(str_hdr)) + sizeof(str_hdr);

    /*
     * 1: ref
     */
    str_hdr.bsize = htole32(sizeof(alias) - 1);
    p = memcpy(p, &str_hdr, sizeof(str_hdr)) + sizeof(str_hdr);
    p = memcpy(p, alias, sizeof(alias) - 1) + (sizeof(alias) - 1);

    if (send(sock, buf, sizeof(buf), 0) != sizeof(buf)) {
        fprintf(stderr, "Send failed\n");
        return -1;
    }

    return 0;
}

static void print_echo(const void *msg, size_t msg_size)
{
    const char *p = (const char *)msg;
    ssize_t left = msg_size;

    while (left > sizeof(struct selva_proto_string)) {
        struct selva_proto_string hdr;
        ssize_t bsize;

        memcpy(&hdr, p, sizeof(hdr));
        left -= sizeof(hdr);
        p += sizeof(hdr);

        bsize = le32toh(hdr.bsize);
        if (hdr.type != SELVA_PROTO_STRING ||
            bsize > left) {
            fprintf(stderr, "Invalid string (type: %d bsize: %zd buf_size: %zd)\n",
                    hdr.type, bsize, left);
            return;
        }

        printf("%.*s", (int)bsize, p);
        left -= bsize;
        p += bsize;
    }
    printf("\n");
}

static void print_selva_resolve(const void *msg, size_t msg_size)
{
    size_t i = 0;

    while (i < msg_size) {
        enum selva_proto_data_type type;
        size_t data_len;
        int off;

        off = selva_proto_parse_vtype(msg, msg_size, i, &type, &data_len);
        if (off <= 0) {
            fprintf(stderr, "Err\n");
            return;
        }

        i += off;

        if (type == SELVA_PROTO_ERROR) {
            const char *err_msg_str;
            size_t err_msg_len;
            int err, err1;

            err = selva_proto_parse_error(msg, msg_size, i - off, &err1, &err_msg_str, &err_msg_len);
            if (err) {
                fprintf(stderr, "Failed to parse an error received: %s\n", selva_strerror(err));
            } else {
                fprintf(stderr, "%.*s: %s\n",
                        (int)err_msg_len, err_msg_str,
                        selva_strerror(err1));
            }
        } else if (type == SELVA_PROTO_STRING) {
            printf("%.*s", (int)data_len, (char *)msg + i - data_len);
        } else if (type == SELVA_PROTO_ARRAY || type == SELVA_PROTO_ARRAY_END) {
            /* NOP */
        } else if (type == SELVA_PROTO_NULL) {
            printf("ENOENT\n");
        } else {
            fprintf(stderr, "Unexpected response value type: %d\n", (int)type);
            return;
        }
    }
}

int main(int argc, char const* argv[])
{
     int sock = connect_to_server();

     if (sock == -1) {
         exit(EXIT_FAILURE);
     }

     for (cmd_fn cmd = get_cmd(); cmd != quit; cmd = get_cmd()) {
         int resp_cmd;
         size_t msg_size;
         void *msg;

         if (!cmd) {
             continue;
         } else if (cmd(sock) == -1) {
             continue;
         }

         msg = recv_message(sock, &resp_cmd, &msg_size);
         if (!msg) {
             fprintf(stderr, "Reading response failed\n");
             continue;
         }

         switch (resp_cmd) {
         case SELVA_CMD_PING:
             if (msg_size >= sizeof(struct selva_proto_control)) {
                 struct selva_proto_control *ctrl = (struct selva_proto_control *)msg;

                 if (ctrl->type == SELVA_PROTO_STRING && msg_size >= sizeof(struct selva_proto_string)) {
                     struct selva_proto_string *s = (struct selva_proto_string *)msg;

                     if (le32toh(s->bsize) <= msg_size - sizeof(struct selva_proto_string)) {
                        printf("%.*s\n", (int)s->bsize, s->data);
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
         case SELVA_CMD_ECHO:
             print_echo(msg, msg_size);
             break;
         case SELVA_CMD_RESOLVE_NODEID:
             print_selva_resolve(msg, msg_size);
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
    eztrie_insert(&commands, "quit", quit);
    eztrie_insert(&commands, "ping", ping);
    eztrie_insert(&commands, "echo", echo);
    eztrie_insert(&commands, "resolve.nodeid", selva_resolve);
}
