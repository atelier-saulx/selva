/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cdefs.h"
#include "endian.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "commands.h"

#define TAB_WIDTH 2
#define TABS_MAX (80 / TAB_WIDTH / 2)

static int cmd_ping_req(const struct cmd *cmd, int sock, int seqno);
static void cmd_ping_res(const struct cmd *cmd, const void *msg, size_t msg_size);
static int cmd_echo_req(const struct cmd *cmd, int sock, int seqno);
static void cmd_echo_res(const struct cmd *cmd, const void *msg, size_t msg_size);
static int cmd_lscmd_req(const struct cmd *cmd, int sock, int seqno);
static void generic_res(const struct cmd *cmd, const void *msg, size_t msg_size);

static struct cmd commands[254] = {
    [0] = {
        .cmd_id = 0,
        .cmd_name = "ping",
        .cmd_req = cmd_ping_req,
        .cmd_res = cmd_ping_res,
    },
    [1] = {
        .cmd_id = 1,
        .cmd_name = "echo",
        .cmd_req = cmd_echo_req,
        .cmd_res = cmd_echo_res,
    },
    [2] = {
        .cmd_id = 2,
        .cmd_name = "lscmd",
        .cmd_req = cmd_lscmd_req,
        .cmd_res = generic_res,
    },
};

static int cmd_ping_req(const struct cmd *cmd, int sock, int seqno)
{
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = cmd->cmd_id;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;

    if (send(sock, buf, sizeof(buf), 0) != sizeof(buf)) {
        fprintf(stderr, "Send failed\n");
        return -1;
    }

    return 0;
}

static void cmd_ping_res(const struct cmd *, const void *msg, size_t msg_size)
{
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
}

static int cmd_echo_req(const struct cmd *cmd, int sock, int seqno)
{
    const char data[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
    const struct selva_proto_string str_hdr = {
        .type = SELVA_PROTO_STRING,
        .bsize = htole32(sizeof(data)),
    };
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header) + sizeof(str_hdr) + sizeof(data)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;
    const int seq = htole32(seqno);
    const int n = 100;

    memcpy(buf + sizeof(*hdr), &str_hdr, sizeof(str_hdr));
    memcpy(buf + sizeof(*hdr) + sizeof(str_hdr), data, sizeof(data));

    for (int i = 0; i < n; i++) {
#if __linux__
        const int send_flags = i < n - 1 ? MSG_MORE : 0;
#else
        const int send_flags = 0;
#endif

        memset(hdr, 0, sizeof(*hdr));
        hdr->cmd = cmd->cmd_id;
        hdr->flags = (i == 0) ? SELVA_PROTO_HDR_FFIRST : (i == n - 1) ? SELVA_PROTO_HDR_FLAST : 0;
        hdr->seqno = seq;
        hdr->frame_bsize = htole16(sizeof(buf));
        hdr->msg_bsize = 0;

        if (send(sock, buf, sizeof(buf), send_flags) != sizeof(buf)) {
            fprintf(stderr, "Send %d/%d failed\n", i, n);
        }
    }

    return 0;
}

static void cmd_echo_res(const struct cmd *, const void *msg, size_t msg_size)
{
    const char *p = (const char *)msg;
    ssize_t left = msg_size;

    while (left > (typeof(left))sizeof(struct selva_proto_string)) {
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

static int cmd_lscmd_req(const struct cmd *cmd, int sock, int seqno)
{
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = cmd->cmd_id;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;

    if (send(sock, buf, sizeof(buf), 0) != sizeof(buf)) {
        fprintf(stderr, "Send failed\n");
        return -1;
    }

    return 0;
}

static int generic_req(const struct cmd *cmd, int sock, int seqno)
{
    return 0;
}

static void generic_res(const struct cmd *cmd, const void *msg, size_t msg_size)
{
    unsigned int tabs_hold_stack[TABS_MAX + 1] = { 0 };
    unsigned int tabs = 0;
    size_t i = 0;

    while (i < msg_size) {
        enum selva_proto_data_type type;
        size_t data_len;
        int off;

        off = selva_proto_parse_vtype(msg, msg_size, i, &type, &data_len);
        if (off <= 0) {
            if (off < 0) {
                fprintf(stderr, "Failed to parse a value header: %s\n", selva_strerror(off));
            }
            return;
        }

        i += off;

        if (type == SELVA_PROTO_NULL) {
            printf("%*s<null>\n", tabs * TAB_WIDTH, "");
        } else if (type == SELVA_PROTO_ERROR) {
            const char *err_msg_str;
            size_t err_msg_len;
            int err, err1;

            err = selva_proto_parse_error(msg, msg_size, i - off, &err1, &err_msg_str, &err_msg_len);
            if (err) {
                fprintf(stderr, "Failed to parse an error received: %s\n", selva_strerror(err));
                return;
            } else {
                printf("%*s<Error %.*s: %s>\n",
                       tabs * TAB_WIDTH, "",
                       (int)err_msg_len, err_msg_str,
                       selva_strerror(err1));
            }
        } else if (type == SELVA_PROTO_DOUBLE) {
            double d;

            memcpy(&d, (char *)msg + i - sizeof(d), sizeof(d));
            /* TODO ledouble to host double */
            printf("%*s%e\n", tabs * TAB_WIDTH, "", d);
        } else if (type == SELVA_PROTO_LONGLONG) {
            uint64_t ll;

            memcpy(&ll, (char *)msg + i - sizeof(ll), sizeof(ll));
            printf("%*s%" PRIu64 "\n", tabs * TAB_WIDTH, "", le64toh(ll));
        } else if (type == SELVA_PROTO_STRING) {
            printf("%*s%.*s\n", tabs * TAB_WIDTH, "", (int)data_len, (char *)msg + i - data_len);
        } else if (type == SELVA_PROTO_ARRAY) {
            struct selva_proto_array hdr;

            memcpy(&hdr, msg + i - off, sizeof(hdr));

            printf("%*s[\n", tabs * TAB_WIDTH, "");
            if (tabs < TABS_MAX) {
                tabs++;
            }

            /* TODO Support embedded arrays */

            if (!(hdr.flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH)) {
                tabs_hold_stack[tabs] = data_len;
                continue; /* Avoid decrementing the tab stack value. */
            }
        } else if (type == SELVA_PROTO_ARRAY_END) {
            if (tabs_hold_stack[tabs]) {
                /*
                 * This isn't necessary if the server is sending correct data.
                 */
                tabs_hold_stack[tabs] = 0;
            }
            if (tabs > 0) {
                tabs--;
            }
            printf("%*s]\n", tabs * TAB_WIDTH, "");
        } else {
            fprintf(stderr, "Invalid proto value\n");
            return;
        }

        /*
         * Handle tabs for fixed size arrays.
         */
        if (tabs_hold_stack[tabs] > 0) {
            tabs_hold_stack[tabs]--;
            if (tabs_hold_stack[tabs] == 0) {
                if (tabs > 0) {
                    tabs--;
                }
                printf("%*s]\n", tabs * TAB_WIDTH, "");
            }
        }
    }
}

void cmd_discover(int fd, void (*cb)(struct cmd *cmd))
{
    /* TODO Discover commands from the server */

    for (struct cmd *cmd = &commands[0]; cmd != commands + num_elem(commands); cmd++) {
        if (cmd->cmd_name && cmd->cmd_req) {
            cb(cmd);
        }
    }
}
