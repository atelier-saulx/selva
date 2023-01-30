/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <alloca.h>
#include <arpa/inet.h>
#include <ctype.h>
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

#define MAX_LINE 200

/* TODO REMOVE */

static struct eztrie commands;
static int seqno = 0;

[[nodiscard]]
static int connect_to_server(const char *addr, int port)
{
    int sock;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Could not create a socket\n");
        return -1;
    }

    (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) == -1) {
        fprintf(stderr, "Invalid address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        fprintf(stderr, "Connection to %s:%d failed\n", addr, port);
        return -1;
    }

    return sock;
}

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

int main(int argc, char *argv[])
{
    int c;
    char *addr = "127.0.0.1";
    int port = 3000;
    static char line[MAX_LINE];
    static char *args[256];
    int sock;

    opterr = 0;
    while ((c = getopt(argc, argv, "p:")) != -1) {
        switch (c) {
        case 'p':
            port = (int)strtol(optarg, NULL, 10);
            break;
        case '?':
            if (optopt == 'p') {
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            } else if (isprint(optopt)) {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            } else {
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            }
            return 1;
        default:
            abort();
        }
    }

    if (optind < argc) {
        addr = argv[optind];
    }

    sock = connect_to_server(addr, port);
    if (sock == -1) {
        exit(EXIT_FAILURE);
    }

    /*
     * Init commands trie.
     */
    eztrie_init(&commands);
    eztrie_insert(&commands, "quit", main);
    cmd_discover(sock, seqno++);
    cmd_foreach(insert_cmd);

    fflush(NULL);
    while (get_line(line, sizeof(line))) {
        int args_c;
        const struct cmd *cmd;

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
        if ((void *)cmd == main) {
            break;
        } else if (cmd && cmd->cmd_id == 254) { /* !listen pseudo-command */
            /* NOP */
        } else if (!cmd || !cmd->cmd_req) {
            fprintf(stderr, "Unknown command\n");
            continue;
        } else if (cmd->cmd_req(cmd, sock, seqno++, args_c, args) == -1) {
            fprintf(stderr, "Command failed\n");
            continue;
        }

        recv_message(sock);
    }

    close(sock);
    return EXIT_SUCCESS;
}
