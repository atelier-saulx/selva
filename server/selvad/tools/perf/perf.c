/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "cdefs.h"
#include "endian.h"
#include "util/crc32c.h"
#include "util/ctime.h"
#include "util/timestamp.h"
#include "selva_error.h"
#include "selva_proto.h"

static int flag_stop;

static void sigint_handler(int sig __unused)
{
    flag_stop = 1;
}

static double ts2ms(struct timespec *ts)
{
    return (double)ts->tv_sec * 1000.0 + (double)ts->tv_nsec / 1.0e6;
}

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

static int send_message(int fd, const void *buf, size_t size, int flags)
{
    if (send(fd, buf, size, flags) != (ssize_t)size) {
        fprintf(stderr, "Send failed\n");
        return -1; /* TODO Maybe an error code? */
    }

    return 0;
}

static int send_modify(int fd, int seqno)
{
    struct {
        struct selva_proto_header hdr;
        struct selva_proto_array arr;
        struct selva_proto_string id_hdr;
        char id_str[10];
        struct selva_proto_string flags_hdr;
        struct selva_proto_string type1_hdr;
        char type1_str[1];
        struct selva_proto_string field1_hdr;
        char field1_str[5];
        struct selva_proto_string value1_hdr;
        char value1_str[3];
    } __packed buf = {
        .hdr = {
            .cmd = 65, /* TODO hardcoded cmd_id */
            .flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
            .seqno = htole32(seqno),
            .frame_bsize = htole16(sizeof(buf)),
            .msg_bsize = 0,
            .chk = 0,
        },
        .arr = {
            .type = SELVA_PROTO_ARRAY,
            .flags = 0,
            .length = htole32(2 + 3),
        },
        .id_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(10),
        },
        .flags_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(0),
        },
        .type1_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.type1_str)),
        },
        .type1_str = {'0'},
        .field1_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.field1_str)),
        },
        .field1_str = {'f', 'i', 'e', 'l', 'd'},
        .value1_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.value1_str)),
        },
        .value1_str = {'l', 'o', 'l'},
    };

    snprintf(buf.id_str, 10, "ma%d", seqno);

    buf.hdr.chk = htole32(crc32c(0, &buf, sizeof(buf)));

    return send_message(fd, &buf, sizeof(buf), 0);
}

static void handle_response(struct selva_proto_header *resp_hdr, void *msg, size_t msg_size)
{
    if (resp_hdr->cmd < 0) {
        fprintf(stderr, "Invalid cmd_id: %d\n", resp_hdr->cmd);
    } else {
        /* NOP */
    }
}

static ssize_t recvn(int fd, void *buf, size_t n)
{
    ssize_t i = 0;

    while (i < (ssize_t)n) {
        ssize_t res;

        errno = 0;
        res = recv(fd, (char *)buf + i, n - i, 0);
        if (res <= 0) {
            if (errno == EINTR) {
                if (flag_stop) {
                    fprintf(stderr, "Interrupted\n");
                    return res;
                }
                continue;
            }

            return res;
        }

        i += res;
    }

    return (ssize_t)i;
}

void recv_message(int fd)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[100 * 1048576] __lazy_alloc_glob;
    struct selva_proto_header resp_hdr;
    size_t i = 0;

    do {
        ssize_t r;

        r = recvn(fd, &resp_hdr, sizeof(resp_hdr));
        if (r != (ssize_t)sizeof(resp_hdr)) {
            fprintf(stderr, "Reading selva_proto header failed. result: %d\n", (int)r);
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
                r = recvn(fd, msg_buf + i, payload_size);
                if (r != (ssize_t)payload_size) {
                    fprintf(stderr, "Reading payload failed: result: %d expected: %d\n", (int)r, (int)payload_size);
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

int main(int argc, char *argv[])
{
    int c;
    char *addr = "127.0.0.1";
    int port = 3000;
    int sock;
    int n = 1000000;
    int seqno = 0;

    opterr = 0;
    while ((c = getopt(argc, argv, "p:N:")) != -1) {
        switch (c) {
        case 'p':
            port = (int)strtol(optarg, NULL, 10);
            break;
        case 'N':
            n = (int)strtol(optarg, NULL, 10);
            break;
        case '?':
            if (optopt == 'p' || optopt == 'N') {
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

    (void)sigaction(SIGINT, &(const struct sigaction){
            .sa_handler = sigint_handler,
            .sa_flags = SA_NODEFER | SA_RESETHAND
            }, NULL);

    struct timespec ts_start, ts_end, ts_diff;
    double t, v;
    const char *unit = "ms";

    ts_monotime(&ts_start);
    while (!flag_stop && seqno < n) {
        send_modify(sock, seqno++);
        recv_message(sock);
    }
    ts_monotime(&ts_end);
    timespec_sub(&ts_diff, &ts_end, &ts_start);
    t = ts2ms(&ts_diff);

    v = ((double)n / t) * 1000.0;
    if (t > 1000.0) {
        t /= 1000.0;
        unit = "s";
    }

    printf("N: %d modify commands\nt: %.2f %s\nv: %.0f modify/s\n",
           n,
           t, unit,
           v);

    close(sock);
    return EXIT_SUCCESS;
}
