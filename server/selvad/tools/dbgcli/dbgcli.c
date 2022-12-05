/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
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
#include "split.h"
#include "commands.h"

#define PORT 3000
#define MAX_LINE 200

/* TODO REMOVE */

static struct eztrie commands;
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

static int cmd_quit(const struct cmd *, int, int)
{
    return 0;
}

#if 0
static int cmd_resolve(int sock)
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
#endif

static void insert_cmd(struct cmd *cmd)
{
    printf("Adding: %s\n", cmd->cmd_name);
    eztrie_insert(&commands, cmd->cmd_name, cmd);
}

static void clear_crlf(char *buf)
{
    char *c = strpbrk(buf, "\r\n");

    if (c) {
        *c = '\0';
    }
}

static char *get_line(char *buf, size_t bsize)
{
    char *s;

    printf("> ");
    s = fgets(buf, bsize, stdin);
    if (s) {
        clear_crlf(buf);
    }

    return s;
}

static const struct cmd *get_cmd(const char *name)
{
    struct eztrie_iterator it;
    struct eztrie_node_value *v;

    it = eztrie_find(&commands, name);
    v = eztrie_remove_ithead(&it);

    if (v && !strcmp(v->key, name)) {
        return v->p;
    }

    return NULL;
}

int main(int argc, char const* argv[])
{
    static char line[MAX_LINE];
    static char *args[256];
    int sock = connect_to_server();

    if (sock == -1) {
        exit(EXIT_FAILURE);
    }

    /*
     * Init commands trie.
     */
    eztrie_init(&commands);
    eztrie_insert(&commands, "quit", cmd_quit);
    cmd_discover(sock, insert_cmd);

    fflush(NULL);
    while (get_line(line, sizeof(line))) {
        int args_c;
        const struct cmd *cmd;
        int resp_cmd;
        size_t msg_size;
        void *msg;

        if (strlen(line) == 0) {
            continue;
        }
        split(line, args, num_elem(args));
        if (!args[0]) {
            continue;
        }

        /* Count args. */
        args_c = 0;
        while (args[args_c++]);
        args_c--;

        cmd = get_cmd(args[0]);
        if (!cmd || !cmd->cmd_req) {
            fprintf(stderr, "Unknown command\n");
            continue;
        } else if (cmd->cmd_req(cmd, sock, seqno++, args_c, args) == -1) {
            fprintf(stderr, "Command failed\n");
            continue;
        }

        msg = recv_message(sock, &resp_cmd, &msg_size);
        if (!msg) {
            fprintf(stderr, "Reading response failed\n");
            continue;
        }

        if (cmd->cmd_res) {
            cmd->cmd_res(cmd, msg, msg_size);
        } else {
            fprintf(stderr, "Unsupported command response\n");
        }
    }

    close(sock);
    return EXIT_SUCCESS;
}
