/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cdefs.h"
#include "endian.h"
#include "selva_proto.h"
#include "util/crc32c.h"
#include "commands.h"

static int send_cmd_head(int sock, int cmd_id, int seqno, const char * restrict node_id, const char * restrict flags, int nr_fields)
{
    const size_t flags_len = strlen(flags);
    struct {
        struct selva_proto_header hdr;
        struct selva_proto_array arr;
        struct selva_proto_string id;
        char id_str[10];
        struct selva_proto_string flags;
    } __packed buf = {
        .hdr = {
            .cmd = cmd_id,
            .flags = SELVA_PROTO_HDR_FFIRST,
            .seqno = htole32(seqno),
            .frame_bsize = htole16(sizeof(buf) + flags_len),
            .msg_bsize = 0,
            .chk = 0,
        },
        .arr = {
            .type = SELVA_PROTO_ARRAY,
            .flags = 0,
            .length = htole32(2 + 3 * nr_fields),
        },
        .id = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(10),
        },
        .flags = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(flags_len),
        },
    };
    uint32_t chk;

    memcpy(buf.id_str, node_id, min(strlen(node_id), (size_t)10));
    chk = crc32c(0, &buf, sizeof(buf));
    chk = crc32c(chk, flags, flags_len);
    buf.hdr.chk = htole32(chk);

    if (send_message(sock, &buf, sizeof(buf), MSG_MORE) ||
        send_message(sock, flags, flags_len, 0)
       ) {
        return -1;
    }
    return 0;
}

static int send_string(int sock, int cmd_id, int seqno, const char *str)
{
    const size_t str_len = strlen(str);
    struct {
        struct selva_proto_header hdr;
        struct selva_proto_string str;
    } __packed buf = {
        .hdr = {
            .cmd = cmd_id,
            .flags = 0,
            .seqno = htole32(seqno),
            .frame_bsize = htole16(sizeof(buf) + str_len),
            .msg_bsize = 0,
            .chk = 0,
        },
        .str = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(str_len),
        },
    };
    uint32_t chk;

    chk = crc32c(0, &buf, sizeof(buf));
    chk = crc32c(chk, str, str_len);
    buf.hdr.chk = htole32(chk);

    if (send_message(sock, &buf, sizeof(buf), MSG_MORE) ||
        send_message(sock, str, str_len, MSG_MORE)
       ) {
        return -1;
    }
    return 0;
}

static int send_terminator(int sock, int cmd_id, int seqno)
{
    struct selva_proto_header hdr = {
        .cmd = cmd_id,
        .flags = SELVA_PROTO_HDR_FLAST,
        .seqno = htole32(seqno),
        .frame_bsize = htole16(sizeof(hdr)),
        .msg_bsize = 0,
        .chk = 0,
    };

    hdr.chk = htole32(crc32c(0, &hdr, sizeof(hdr)));
    return send_message(sock, &hdr, sizeof(hdr), 0);
}

static int cmd_modify_req(int sock, int cmd_id, int seqno, const char *node_id, char type, int argc, char *argv[])
{
    int err;
    const char *flags = "";
    const size_t nr_fields = argc / 2;

    err = send_cmd_head(sock, cmd_id, seqno, node_id, flags, nr_fields);
    if (err) {
        return err;
    }

    for (int i = 0; i < argc; i += 2) {
        const char type_str[] = { type, '\0' };

        if (send_string(sock, cmd_id, seqno, type_str) ||
            send_string(sock, cmd_id, seqno, argv[i]) ||
            send_string(sock, cmd_id, seqno, argv[i + 1])) {
            return -1;
        }
    }

    return send_terminator(sock, cmd_id, seqno);
}

int cmd_modify_string_req(const struct cmd *cmd, int sock, int seqno, int argc, char *argv[])
{
    if (argc < 2 || (argc - 2) % 2) {
        fprintf(stderr, "Invalid arguments\n");
        return -1;
    }

    const char *node_id = argv[1];
    return cmd_modify_req(sock, cmd->cmd_id, seqno, node_id, '0', argc - 2, argv + 2);
}
