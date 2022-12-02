#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "cdefs.h"
#include "endian.h"
#include "selva_proto.h"
#include "commands.h"

#define TAB_WIDTH 2

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

static void cmd_ping_res(const struct cmd *cmd, const void *msg, size_t msg_size)
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

static void cmd_echo_res(const struct cmd *cmd, const void *msg, size_t msg_size)
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
    const char *p = (const char *)msg;
    ssize_t left = msg_size;
#define CPY_HDR(_hdr_) \
    do { \
        (void)(_hdr_).type; /* Fail comp if pointer. */ \
        memcpy(&(_hdr_), p, sizeof(_hdr_)); \
        left -= sizeof(_hdr_); \
        p += sizeof(hdr); \
    } while (0)
    const unsigned int tabs_max = 80 / TAB_WIDTH / 2;
    unsigned int tabs_hold_stack[tabs_max + 1] = { 0 };
    unsigned int tabs = 0;

    while (left > sizeof(struct selva_proto_control)) {
        struct selva_proto_control hdr;
        ssize_t bsize;

        memcpy(&hdr, p, sizeof(hdr));
        if (hdr.type == SELVA_PROTO_NULL) {
            left -= sizeof(struct selva_proto_null);
            p += sizeof(struct selva_proto_null);

            printf("%*s<null>\n", tabs * TAB_WIDTH, "");
        } else if (hdr.type == SELVA_PROTO_ERROR) {
            const char *err_msg_str;
            size_t err_msg_len;
            int err, err1;

            err = selva_proto_parse_error(msg, msg_size, i - off, &err1, &err_msg_str, &err_msg_len);
            if (err) {
                fprintf(stderr, "Failed to parse an error received: %s\n", selva_strerror(err));
                return;
            } else {
                fprintf("%*s<Error %.*s: %s>\n",
                        tabs * TAB_WIDTH, "",
                        (int)err_msg_len, err_msg_str,
                        selva_strerror(err1));
            }
        } else if (hdr.type == SELVA_PROTO_DOUBLE) {
        } else if (hdr.type == SELVA_PROTO_LONGLONG) {
        } else if (hdr.type == SELVA_PROTO_STRING) {
            struct selva_proto_string str_hdr;

            if (bsize < sizeof(struct selva_proto_string)) {
                fprintf(stderr, "Invalid proto string header\n");
                return;
            }
            CPY_HDR(str_hdr);

            hdr.bsize = bsize = le32toh(hdr.bsize);
            if (hdr.bsize > left) {
                fprintf(stderr, "Invalid proto string size\n");
                return;
            }

            printf("%*s%.*s\n", tabs * TAB_WIDTH, "", (int)hdr.bsize, p);
        } else if (hdr.type == SELVA_PROTO_ARRAY) {
            printf("%*s[\n", tabs * TAB_WIDTH, "");
            if (tabs < tabs_max) {
                tabs++;
            }

            if (!(array_hdr.flags & SELVA_PROTO_ARRAY_FPOSTPONED_LENGTH)) {
                tabs_hold_stack[tabs] = array_hdr.length;
            }
        } else if (hdr.type == SELVA_PROTO_ARRAY_END) {
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

        left -= bsize;
        p += bsize;
    }
    printf("\n");
#undef CPY_HDR
#undef PRINT_TABS
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
