/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "event_loop.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "util/crc32c.h"
#include "../../../commands.h"
#include "replicaof.h"

int replication_replica_connect_to_origin(struct sockaddr_in *origin_addr)
{
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Could not create a socket");
        return SELVA_ENOBUFS;
    }

    (void)setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));

    if (connect(sock, (struct sockaddr*)origin_addr, sizeof(*origin_addr)) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed");
        return SELVA_EIO;
    }

    return sock;
}

static int send_sync_req(int sock)
{
    const int seqno = 0;
    _Alignas(struct selva_proto_header) char buf[sizeof(struct selva_proto_header)];
    struct selva_proto_header *hdr = (struct selva_proto_header *)buf;

    memset(hdr, 0, sizeof(*hdr));
    hdr->cmd = CMD_REPLICASYNC_ID;
    hdr->flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST;
    hdr->seqno = htole32(seqno);
    hdr->frame_bsize = htole16(sizeof(buf));
    hdr->msg_bsize = 0;
    hdr->chk = htole32(crc32c(0, buf, sizeof(buf)));

    if (send(sock, buf, sizeof(buf), 0) != (ssize_t)sizeof(buf)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Sending REPLICASYNC request failed");
        return SELVA_EIO;
    }

    return 0;
}

static void on_data(struct event *event, void *arg)
{
    static _Alignas(uintptr_t) uint8_t msg_buf[1048576];
    const int fd = event->fd;
    size_t i = 0;
    int8_t cmd_id;
    size_t replication_data_size;

    do {
        struct selva_proto_header hdr;

        if (recv(fd, &hdr, sizeof(hdr), 0) != (ssize_t)sizeof(hdr)) {
            /* TODO error handling */
            SELVA_LOG(SELVA_LOGL_ERR, "Invalid header");
            return;
        } else {
            const size_t payload_size = le16toh(hdr.frame_bsize) - sizeof(hdr);

            if (!(hdr.flags & (SELVA_PROTO_HDR_FREQ_RES | SELVA_PROTO_HDR_STREAM))) {
                SELVA_LOG(SELVA_LOGL_ERR, "Unexpected frame flags: 0x%x", hdr.flags);
                return;
            } else if (hdr.cmd != CMD_REPLICASYNC_ID) {
                SELVA_LOG(SELVA_LOGL_ERR, "Unexpected command. received: %d expected: %d", hdr.cmd, CMD_REPLICASYNC_ID);
                return;
            } else if (i + payload_size > sizeof(msg_buf)) {
                /* TODO */
                SELVA_LOG(SELVA_LOGL_ERR, "Buffer overflow");
                return;
            }

            if (payload_size > 0) {
                if (recv(fd, msg_buf + i, payload_size, 0) != (ssize_t)payload_size) {
                    /* TODO Error handling */
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to receive a payload");
                    return;
                }
            }

            if (!selva_proto_verify_frame_chk(&hdr, msg_buf + i, payload_size)) {
                SELVA_LOG(SELVA_LOGL_ERR, "Frame checksum mismatch");
                return;
            }

            if (payload_size == 0) {
                continue;
            }

            if (i == 0) {
                size_t size = payload_size - sizeof(struct selva_proto_replication);
                int err;

                err = selva_proto_parse_replication(msg_buf, payload_size, 0, &cmd_id, &replication_data_size);
                if (err) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse the replication header: %s", selva_strerror(err));
                    return;
                }

                memmove(msg_buf, msg_buf + sizeof(struct selva_proto_replication), size);
                i += size;
            } else {
                i += payload_size;
            }

            if (hdr.flags & SELVA_PROTO_HDR_FLAST && i < replication_data_size) {
                /*
                 * Replication command not fully read and the stream was terminated abruptly.
                 */
                SELVA_LOG(SELVA_LOGL_ERR, "Stream terminated abruptly");
                /* TODO Error handling */
                return;
            }
        }
    } while (i < replication_data_size);

    if (i != replication_data_size) {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication payload size");
        return;
    }

    /* TODO Run command */
    SELVA_LOG(SELVA_LOGL_INFO, "Replicating %d\n", cmd_id);
    selva_server_run_cmd(cmd_id, msg_buf, replication_data_size);
}

static void on_close(struct event *event, void *arg)
{
    const int fd = event->fd;

    /* TODO */
    SELVA_LOG(SELVA_LOGL_WARN, "Replication has stopped due to connection reset");

    (void)shutdown(fd, SHUT_RDWR);
    close(fd);
}

int replication_replica_start(int sock)
{
    int err;

    err = send_sync_req(sock);
    if (err) {
        return err;
    }

    err = evl_wait_fd(sock, on_data, NULL, on_close, NULL);
    return err;
}
