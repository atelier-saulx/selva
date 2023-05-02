/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
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
#include "../../commands.h"

#define NODE_ID_SIZE 16

struct thread_args {
    int fd;
    int n;
};

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

static int send_modify(int fd, int seqno, typeof_field(struct selva_proto_header, flags) frame_extra_flags, char node_id[NODE_ID_SIZE])
{
    struct {
        struct selva_proto_header hdr;
        struct selva_proto_array arr;
        struct selva_proto_string id_hdr;
        char id_str[NODE_ID_SIZE];
        struct selva_proto_string flags_hdr;
        /* field 1 */
        struct selva_proto_string type1_hdr;
        char type1_str[1];
        struct selva_proto_string field1_hdr;
        char field1_str[5];
        struct selva_proto_string value1_hdr;
        char value1_str[3];
        /* field 2 */
        struct selva_proto_string type2_hdr;
        char type2_str[1];
        struct selva_proto_string field2_hdr;
        char field2_str[3];
        struct selva_proto_string value2_hdr;
        char value2_str[sizeof(uint64_t)];
    } __packed buf = {
        .hdr = {
            .cmd = CMD_ID_MODIFY,
            .flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST | frame_extra_flags,
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
            .bsize = htole32(NODE_ID_SIZE),
        },
        .flags_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(0),
        },
        /* Field 1 */
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
        /* Field 2 */
        .type2_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.type2_str)),
        },
        .type2_str = {'3'},
        .field2_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.field2_str)),
        },
        .field2_str = {'n', 'u', 'm'},
        .value2_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
            .bsize = htole32(sizeof(buf.value2_str)),
        },
    };

    strncpy(buf.id_str, node_id, NODE_ID_SIZE);
    memcpy(buf.value2_str, &(uint64_t){seqno}, sizeof(uint64_t));

    buf.hdr.chk = htole32(crc32c(0, &buf, sizeof(buf)));

    return send_message(fd, &buf, sizeof(buf), 0);
}

static void handle_response(struct selva_proto_header *resp_hdr, void *msg __unused, size_t msg_size __unused)
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

int recv_message(int fd)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[100 * 1048576] __lazy_alloc_glob;
    struct selva_proto_header resp_hdr;
    size_t i = 0;

    do {
        ssize_t r;

        r = recvn(fd, &resp_hdr, sizeof(resp_hdr));
        if (r != (ssize_t)sizeof(resp_hdr)) {
            fprintf(stderr, "Reading selva_proto header failed. result: %d\n", (int)r);
            return 1;
        } else {
            size_t frame_bsize = le16toh(resp_hdr.frame_bsize);
            const size_t payload_size = frame_bsize - sizeof(resp_hdr);

            if (!(resp_hdr.flags & SELVA_PROTO_HDR_FREQ_RES)) {
                fprintf(stderr, "Invalid response: response bit not set\n");
                return 1;
            } else if (i + payload_size > sizeof(msg_buf)) {
                fprintf(stderr, "Buffer overflow\n");
                return 1;
            }

            if (payload_size > 0) {
                r = recvn(fd, msg_buf + i, payload_size);
                if (r != (ssize_t)payload_size) {
                    fprintf(stderr, "Reading payload failed: result: %d expected: %d\n", (int)r, (int)payload_size);
                    return 1;
                }

                i += payload_size;
            }

            if (!selva_proto_verify_frame_chk(&resp_hdr, msg_buf + i - payload_size, payload_size)) {
                fprintf(stderr, "Checksum mismatch\n");
                return 1;
            }
        }

        /*
         * Note that we don't handle multiplexing or any kind of interleaved
         * responses here. We are just expecting that the server is only sending
         * us responses to a single command.
         */
        if (resp_hdr.flags & SELVA_PROTO_HDR_STREAM) {
            if (resp_hdr.flags & SELVA_PROTO_HDR_FLAST) {
                return 0;
            }

            handle_response(&resp_hdr, msg_buf, i);
            i = 0;
        }
    } while (!(resp_hdr.flags & SELVA_PROTO_HDR_FLAST));

    handle_response(&resp_hdr, msg_buf, i);
    return 0;
}

void *recv_thread(void *arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    int fd = args->fd;
    int n = args->n;

    while (!flag_stop && n-- && !recv_message(fd));

    return NULL;
}

pthread_t start_recv(int fd, int n)
{
    pthread_t thread;
    static struct thread_args args;

    args.fd = fd;
    args.n = n;

    if (pthread_create(&thread, NULL, &recv_thread, &args)) {
        fprintf(stderr, "Failed to create a thread\n");
        exit(EXIT_FAILURE);
    }

    return thread;
}

int main(int argc, char *argv[])
{
    int c;
    char *addr = "127.0.0.1";
    int port = 3000;
    int sock;
    int seqno = 0;
    int batch = 0;
    int threaded = 0;
    int single_node = 0;
    int n = 1000000;


    opterr = 0;
    while ((c = getopt(argc, argv, "N:p:bts")) != -1) {
        switch (c) {
        case '?':
            if (optopt == 'p' || optopt == 'N') {
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            } else if (isprint(optopt)) {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            } else {
                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
            }
            return 1;
        case 'N':
            n = (int)strtol(optarg, NULL, 10);
            break;
        case 'b':
            batch = 1;
            break;
        case 'p':
            port = (int)strtol(optarg, NULL, 10);
            break;
        case 's':
            single_node = 1;
            break;
        case 't':
            threaded = 1;
            break;
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

    pthread_t thread;
    struct timespec ts_start, ts_end, ts_diff;
    double t, v;
    const char *unit = "ms";

    if (threaded) {
        thread = start_recv(sock, n);
    }

    ts_monotime(&ts_start);
    while (!flag_stop && seqno < n) {
        typeof_field(struct selva_proto_header, flags) frame_extra_flags = (batch && (seqno < n - 1)) ? SELVA_PROTO_HDR_BATCH : 0;
        char node_id[NODE_ID_SIZE + 1];

        snprintf(node_id, sizeof(node_id), "ma%.*x", NODE_ID_SIZE - 2, single_node ? 1 : seqno);

        send_modify(sock, seqno++, frame_extra_flags, node_id);
        if (!threaded) {
            flag_stop |= recv_message(sock);
        }
    }
    ts_monotime(&ts_end);
    timespec_sub(&ts_diff, &ts_end, &ts_start);
    t = ts2ms(&ts_diff);

    v = ((double)seqno / t) * 1000.0;
    if (t > 1000.0) {
        t /= 1000.0;
        unit = "s";
    }

    if (threaded) {
        pthread_join(thread, NULL);
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);

    printf("N: %d modify commands\nt: %.2f %s\nv: %.0f modify/s\n",
           seqno,
           t, unit,
           v);

    return EXIT_SUCCESS;
}
