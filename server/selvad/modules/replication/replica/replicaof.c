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
    /* TODO */
    SELVA_LOG(SELVA_LOGL_WARN, "Received replication data");
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
