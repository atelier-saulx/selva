/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define __STDC_FORMAT_MACROS 1
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cdefs.h"
#include "endian.h"
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_proto.h"
#include "util/crc32c.h"
#include "commands.h"

#define TAB_WIDTH 2
#define TABS_MAX (80 / TAB_WIDTH / 2)

/*
 * Ping is special because it has no body/payload.
 */
static int cmd_ping_req(const struct cmd *cmd, int sock, int seqno, int argc, char *argv[]);
static void cmd_ping_res(const struct cmd *cmd, const void *msg, size_t msg_size);

/*
 * lscmd request is similar to ping.
 */
static int cmd_lscmd_req(const struct cmd *cmd, int sock, int seqno, int argc, char *argv[]);

/*
 * Currently most commands encode the request arguments using strings and send
 * back more properly formatted responses (using integers, arrays, etc.). This
 * will change in the future once we optimize more things and move away from the
 * Redis ways.
 */
static int generic_req(const struct cmd *cmd, int sock, int seqno, int argc, char *argv[]);
static void generic_res(const struct cmd *cmd, const void *msg, size_t msg_size);

/**
 * A map of Selva commands.
 * Only a few commands are hardcoded here as the others can be "discovered"
 * using `lscmd`.
 */
static struct cmd commands[255] = {
    [0] = {
        .cmd_id = 0,
        .cmd_name = "ping",
        .cmd_req = cmd_ping_req,
        .cmd_res = cmd_ping_res,
    },
    [1] = {
        .cmd_id = 1,
        .cmd_name = "echo",
        .cmd_req = generic_req,
        .cmd_res = generic_res,
    },
    [2] = {
        .cmd_id = 2,
        .cmd_name = "lscmd",
        .cmd_req = cmd_lscmd_req,
        .cmd_res = generic_res,
    },
    [254] = { /* Pseudo-command to read the socket */
        .cmd_id = 254,
        .cmd_name = "!listen",
        .cmd_req = NULL,
        .cmd_res = generic_res,
    }
};

int send_message(int fd, void *buf, size_t size, int flags)
{
    if (send(fd, buf, size, flags) != (ssize_t)size) {
        fprintf(stderr, "Send failed\n");
        return -1; /* TODO Maybe an error code? */
    }

    return 0;
}

static void handle_response(struct selva_proto_header *resp_hdr, void *msg, size_t msg_size)
{
    static_assert((1 << (sizeof(resp_hdr->cmd) * 8)) - 1 <= num_elem(commands));
    if (resp_hdr->cmd < 0) {
        fprintf(stderr, "Invalid cmd_id: %d\n", resp_hdr->cmd);
    } else {
        struct cmd *cmd;

        cmd = &commands[resp_hdr->cmd];
        if (cmd->cmd_res) {
            cmd->cmd_res(cmd, msg, msg_size);
        } else {
            fprintf(stderr, "Unsupported command response\n");
        }
    }
}

static int flag_stop_recv;

static void recv_int_handler(int sig __unused)
{
    flag_stop_recv = 1;
}

void recv_message(int fd)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[1048576];
    struct selva_proto_header resp_hdr;
    size_t i = 0;

    flag_stop_recv = 0;
    (void)sigaction(SIGINT, &(const struct sigaction){
            .sa_handler = recv_int_handler,
            .sa_flags = SA_NODEFER | SA_RESETHAND
            }, NULL);

    do {
        ssize_t r;

        r = recv(fd, &resp_hdr, sizeof(resp_hdr), 0);
        if (r != (ssize_t)sizeof(resp_hdr)) {
            if (errno == EINTR) {
                if (flag_stop_recv) {
                    fprintf(stderr, "Interrupted\n");
                    return;
                }
                continue;
            }
            fprintf(stderr, "recv() returned %d\n", (int)r);
            exit(1);
        } else {
            size_t frame_bsize = le16toh(resp_hdr.frame_bsize);
            const size_t payload_size = frame_bsize - sizeof(resp_hdr);

            if (!(resp_hdr.flags & SELVA_PROTO_HDR_FREQ_RES)) {
                fprintf(stderr, "Invalid response: response bit not set\n");
                return;
            } else if (i + payload_size > sizeof(msg_buf)) {
                fprintf(stderr, "Buffer overflow\n");
                return;
            }

            if (payload_size > 0) {
                r = recv(fd, msg_buf + i, payload_size, 0);
                if (r != (ssize_t)payload_size) {
                    if (errno == EINTR) {
                        if (flag_stop_recv) {
                            fprintf(stderr, "Interrupted\n");
                            return;
                        }
                        continue;
                    }
                    fprintf(stderr, "recv() returned %d\n", (int)r);
                    return;
                }

                i += payload_size;
            }

            if (!selva_proto_verify_frame_chk(&resp_hdr, msg_buf + i - payload_size, payload_size)) {
                fprintf(stderr, "Checksum mismatch\n");
                return;
            }
        }

        /*
         * Note that we don't handle multiplexing or any kind of interleaved
         * responses here. We are just expecting that the server is only sending
         * us responses to a single command.
         */
        if (resp_hdr.flags & SELVA_PROTO_HDR_STREAM) {
            if (resp_hdr.flags & SELVA_PROTO_HDR_FLAST) {
                return;
            }

            handle_response(&resp_hdr, msg_buf, i);
            i = 0;
        }
    } while (!(resp_hdr.flags & SELVA_PROTO_HDR_FLAST));

    handle_response(&resp_hdr, msg_buf, i);
}

static int cmd_ping_req(const struct cmd *cmd, int sock, int seqno, int argc __unused, char *argv[] __unused)
{
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = cmd->cmd_id;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;
    hdr->chk = htole32(crc32c(0, buf, sizeof(buf)));

    if (send_message(sock, buf, sizeof(buf), 0)) {
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
            fprintf(stderr, "ping: Unexpected response value type: %s\n", selva_proto_type_to_str(ctrl->type, NULL));
        }
    } else {
        fprintf(stderr, "Response is shorter than expected\n");
    }
}

static int cmd_lscmd_req(const struct cmd *cmd, int sock, int seqno, int argc __unused, char *argv[] __unused)
{
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = cmd->cmd_id;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;
    hdr->chk = htole32(crc32c(0, buf, sizeof(buf)));

    if (send_message(sock, buf, sizeof(buf), 0)) {
        return -1;
    }

    return 0;
}

static int generic_req(const struct cmd *cmd, int sock, int seqno, int argc, char *argv[])
{
    const int seq = htole32(seqno);
#define FRAME_PAYLOAD_SIZE_MAX (sizeof(struct selva_proto_string) + 20)
    int arg_i = 1;
    int value_i = 0;
    int frame_nr = 0;

    while (arg_i < argc || frame_nr == 0) {
        _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header) + FRAME_PAYLOAD_SIZE_MAX];
        struct selva_proto_header *hdr = (struct selva_proto_header *)buf;
        size_t frame_bsize = sizeof(*hdr);

        memset(hdr, 0, sizeof(*hdr));
        hdr->cmd = cmd->cmd_id;
        hdr->flags = (frame_nr++ == 0) ? SELVA_PROTO_HDR_FFIRST : 0;
        hdr->seqno = seq;
        hdr->msg_bsize = 0;

        while (frame_bsize < sizeof(buf) && arg_i < argc) {
            if (value_i == 0) {
                const struct selva_proto_string str_hdr = {
                    .type = SELVA_PROTO_STRING,
                    .bsize = htole32(strlen(argv[arg_i])),
                };

                if (frame_bsize + sizeof(str_hdr) >= sizeof(buf)) {
                    break;
                }

                memcpy(buf + frame_bsize, &str_hdr, sizeof(str_hdr));
                frame_bsize += sizeof(str_hdr);
            }

            while (frame_bsize < sizeof(buf)) {
                const char c = argv[arg_i][value_i++];

                if (c == '\0') {
                    arg_i++;
                    value_i = 0;
                    break;
                }
                buf[frame_bsize++] = c;
            }
        }
        hdr->frame_bsize = htole16(frame_bsize);

        int send_flags = 0;

        if (arg_i == argc) {
            hdr->flags |= SELVA_PROTO_HDR_FLAST;
        } else {
#if     __linux__
            send_flags = MSG_MORE;
#endif
        }

        hdr->chk = 0;
        hdr->chk = htole32(crc32c(0, buf, frame_bsize));

        send_message(sock, buf, frame_bsize, send_flags);
    }

#undef FRAME_PAYLOAD_SIZE_MAX
    return 0;
}

static void generic_res(const struct cmd *cmd __unused, const void *msg, size_t msg_size)
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
            printf("%*s<null>,\n", tabs * TAB_WIDTH, "");
        } else if (type == SELVA_PROTO_ERROR) {
            const char *err_msg_str;
            size_t err_msg_len;
            int err, err1;

            err = selva_proto_parse_error(msg, msg_size, i - off, &err1, &err_msg_str, &err_msg_len);
            if (err) {
                fprintf(stderr, "Failed to parse an error received: %s\n", selva_strerror(err));
                return;
            } else {
                printf("%*s<Error %s: %.*s>,\n",
                       tabs * TAB_WIDTH, "",
                       selva_strerror(err1),
                       (int)err_msg_len, err_msg_str);
            }
        } else if (type == SELVA_PROTO_DOUBLE) {
            double d;

            memcpy(&d, (char *)msg + i - sizeof(d), sizeof(d));
            /* TODO ledouble to host double */
            printf("%*s%e,\n", tabs * TAB_WIDTH, "", d);
        } else if (type == SELVA_PROTO_LONGLONG) {
            const uint8_t flags = *((const uint8_t *)msg + i - off + offsetof(struct selva_proto_longlong, flags));
            uint64_t ll;

            memcpy(&ll, (char *)msg + i - sizeof(ll), sizeof(ll));
            ll = le64toh(ll);

            if (flags & SELVA_PROTO_LONGLONG_FMT_HEX) {
                printf("%*s%" PRIx64 ",\n", tabs * TAB_WIDTH, "", ll);
            } else {
                printf("%*s%" PRIu64 ",\n", tabs * TAB_WIDTH, "", ll);
            }
        } else if (type == SELVA_PROTO_STRING) {
            struct selva_proto_string str_hdr;

            memcpy(&str_hdr, msg + i - off, sizeof(str_hdr));
            if (str_hdr.flags & SELVA_PROTO_STRING_FBINARY) {
                const char *p = (char *)msg + i - data_len;

                printf("%*s\"", tabs * TAB_WIDTH, "");
                for (size_t data_i = 0; data_i < data_len; data_i++) {
                    printf("%x", p[data_i]);
                }
                printf("\",\n");
            } else {
                printf("%*s\"%.*s\",\n", tabs * TAB_WIDTH, "", (int)data_len, (char *)msg + i - data_len);
            }
        } else if (type == SELVA_PROTO_ARRAY) {
            struct selva_proto_array hdr;

            memcpy(&hdr, msg + i - off, sizeof(hdr));

            /* TODO Support embedded arrays */
            if (hdr.flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH) {
                printf("%*s[\n", tabs * TAB_WIDTH, "");
                if (tabs < TABS_MAX) {
                    tabs++;
                }
            } else {
                if (data_len == 0) {
                    printf("%*s[]\n", tabs * TAB_WIDTH, "");
                } else {
                    printf("%*s[\n", tabs * TAB_WIDTH, "");
                    if (tabs < TABS_MAX) {
                        tabs++;
                    }

                    tabs_hold_stack[tabs] = data_len;
                    continue; /* Avoid decrementing the tab stack value. */
                }
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
        } else if (type == SELVA_PROTO_REPLICATION_CMD) {
            int8_t repl_cmd_id;
            const char *repl_cmd_str;
            char buf[5];
            size_t cmd_size;
            int err;

            err = selva_proto_parse_replication_cmd(msg, msg_size, i - off, &repl_cmd_id, &cmd_size);
            if (err) {
                fprintf(stderr, "Failed to parse an error received: %s\n", selva_strerror(err));
                return;
            }

            repl_cmd_str = commands[repl_cmd_id].cmd_name ?: ({ snprintf(buf, sizeof(buf), "%d", repl_cmd_id); buf; });
            printf("%*s<replication cmd=%s size=%zu>,\n", tabs * TAB_WIDTH, "", repl_cmd_str, cmd_size);
            i = i - off + sizeof(struct selva_proto_replication_cmd);
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

static void cmd_discover_res(const struct cmd *, const void *msg, size_t msg_size)
{
    size_t i = 0;
    int level = 0;
    int cmd_id;

    while (msg && i < msg_size) {
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

        if (level < 2) {
            if (type == SELVA_PROTO_ARRAY) {
                level++;
            } else if (type == SELVA_PROTO_ARRAY_END) {
                printf("Commands discovery complete\n");
                break;
            } else {
                fprintf(stderr, "Invalid response from lscmd\n");
                break;
            }
        } else if (level == 2) {
            if (type == SELVA_PROTO_LONGLONG) {
                uint64_t ll;

                memcpy(&ll, (char *)msg + i - sizeof(ll), sizeof(ll));
                cmd_id = le64toh(ll);
            } else if (type == SELVA_PROTO_STRING) {
                struct cmd *cmd = &commands[cmd_id];

                if (!cmd->cmd_name) {
                    char *cmd_name = selva_malloc(data_len + 1);

                    memcpy(cmd_name, (char *)msg + i - data_len, data_len);
                    cmd_name[data_len] = '\0';

                    cmd->cmd_id = cmd_id;
                    cmd->cmd_name = cmd_name;
                    cmd->cmd_req = generic_req;
                    cmd->cmd_res = generic_res;
                }

                level--;
            } else {
                fprintf(stderr, "Invalid response from lscmd\n");
                break;
            }
        }
    }
}

void cmd_discover(int fd, int seqno)
{
    const size_t id = 2;
    const cmd_res_fn prev_cmd_res = commands[id].cmd_res;

    commands[id].cmd_res = cmd_discover_res;

    if (cmd_lscmd_req(&commands[id], fd, seqno, 0, NULL)) {
        fprintf(stderr, "Commands discovery failed\n");
    } else {
        recv_message(fd);
    }

    commands[id].cmd_res = prev_cmd_res;
}

void cmd_foreach(void (*cb)(struct cmd *cmd))
{
    for (struct cmd *cmd = &commands[0]; cmd != commands + num_elem(commands); cmd++) {
        if (cmd->cmd_name) {
            cb(cmd);
        }
    }
}
