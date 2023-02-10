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
#include "endian.h"
#include "jemalloc.h"
#include "util/crc32c.h"
#include "util/tcp.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "../../../commands.h"
#include "../../../tunables.h"
#include "replication.h"

#if 0
static struct replica_state {
    char sdb_hash[HASH_SIZE]; /*!< The hash of the last known dump. */
    uint64_t sdb_eid; /*!< eid for the hash. */
} replica_state;
#endif

struct replication_sock_state {
    int recv_next_frame;
    enum repl_proto_state {
        REPL_PROTO_STATE_PARSE_REPLICATION_HEADER,
        REPL_PROTO_STATE_RECEIVING_CMD, /*!< Receiving more frames for the replicated command. */
        REPL_PROTO_STATE_RECEIVING_SDB,
        REPL_PROTO_STATE_EXEC_CMD,
        REPL_PROTO_STATE_EXEC_SDB,
        REPL_PROTO_STATE_ERR,
        REPL_PROTO_STATE_FIN,
    } state;
    struct selva_proto_header cur_hdr;
    size_t cur_payload_size; /*! Size of the last received payload. */

    int8_t cmd_id; /*!< Used if replicating a command. */
    size_t cmd_size; /*!< Size of the incoming command in bytes. */

    size_t msg_buf_i;
    _Alignas(uintptr_t) uint8_t msg_buf[1048576];
};

static void init_sv(struct replication_sock_state *sv)
{
    /* TODO Too slow */
    memset(sv, 0, sizeof(*sv));

    /*
     * This should be enough to start over.
     */
    sv->recv_next_frame = 1;
    sv->state = REPL_PROTO_STATE_PARSE_REPLICATION_HEADER;
}


int replication_replica_connect_to_origin(struct sockaddr_in *origin_addr)
{
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Could not create a socket");
        return SELVA_ENOBUFS;
    }

    tcp_set_nodelay(sock);
    tcp_set_keepalive(sock, TCP_KEEPALIVE_TIME, TCP_KEEPALIVE_INTVL, TCP_KEEPALIVE_PROBES);

    if (connect(sock, (struct sockaddr*)origin_addr, sizeof(*origin_addr)) == -1) {
        SELVA_LOG(SELVA_LOGL_ERR, "Connection failed");
        return SELVA_EIO;
    }

    return sock;
}

/* TODO Here we could send what we already have to avoid full sync */
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

static int recv_frame(struct replication_sock_state *sv, int fd)
{
    if (recv(fd, &sv->cur_hdr, sizeof(sv->cur_hdr), 0) != (ssize_t)sizeof(sv->cur_hdr)) {
        /* TODO error handling */
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid selva_proto_header");
        return SELVA_PROTO_EBADF;
    }

    if (!(sv->cur_hdr.flags & (SELVA_PROTO_HDR_FREQ_RES | SELVA_PROTO_HDR_STREAM))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Unexpected frame flags: 0x%x", sv->cur_hdr.flags);
        return SELVA_PROTO_EBADMSG;
    } else if (sv->cur_hdr.cmd != CMD_REPLICASYNC_ID) {
        SELVA_LOG(SELVA_LOGL_ERR, "Unexpected command. received: %d expected: %d", sv->cur_hdr.cmd, CMD_REPLICASYNC_ID);
        return SELVA_PROTO_EBADMSG;
    }

    const size_t payload_size = le16toh(sv->cur_hdr.frame_bsize) - sizeof(sv->cur_hdr);
    if (sv->msg_buf_i + payload_size > sizeof(sv->msg_buf)) {
        /* TODO */
        SELVA_LOG(SELVA_LOGL_ERR, "Buffer overflow");
        return SELVA_PROTO_ENOBUFS;
    } else if (payload_size > 0) {
        if (recv(fd, sv->msg_buf + sv->msg_buf_i, payload_size, 0) != (ssize_t)payload_size) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to receive a payload");
            return SELVA_PROTO_EBADF;
        }
    }

    sv->cur_payload_size = payload_size;

    if (!selva_proto_verify_frame_chk(&sv->cur_hdr, sv->msg_buf + sv->msg_buf_i, payload_size)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Frame checksum mismatch");
        return SELVA_PROTO_EBADMSG;
    }

    sv->recv_next_frame = 0;
    return 0;
}

static enum repl_proto_state parse_replication_header(struct replication_sock_state *sv)
{
    struct selva_proto_control *ctrl = (struct selva_proto_control *)sv->msg_buf;

    /* TODO check if type = err */
    if (ctrl->type == SELVA_PROTO_REPLICATION_CMD) {
        /*
         * The following calculation is only true if the replication header was
         * located in the beginning of a frame, which we assume to be generally
         * true for a valid replication message. Currently we don't even end up
         * to this function in any other case nor ever after the initial frame
         * of a message we are following in the stream using sv->state.
         */
        sv->cur_payload_size -= sizeof(struct selva_proto_replication_cmd);
        int err;

        err = selva_proto_parse_replication_cmd(sv->msg_buf, sizeof(struct selva_proto_replication_cmd), 0, &sv->cmd_id, &sv->cmd_size);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse the replication header: %s", selva_strerror(err));
            return REPL_PROTO_STATE_ERR;
        }

        uint8_t *p = sv->msg_buf + sv->msg_buf_i;
        memmove(p, p + sizeof(struct selva_proto_replication_cmd), sv->cur_payload_size);

         return REPL_PROTO_STATE_RECEIVING_CMD;
    } else if (ctrl->type == SELVA_PROTO_REPLICATION_SDB) {
        return REPL_PROTO_STATE_RECEIVING_SDB;
    }

    /* TODO cleanup? */
    SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication header type: %s", selva_proto_type_to_str(ctrl->type, NULL));

    return REPL_PROTO_STATE_ERR;
}

static enum repl_proto_state handle_recv_cmd(struct replication_sock_state *sv)
{
    sv->msg_buf_i += sv->cur_payload_size;

    if (sv->cmd_size > sv->msg_buf_i) {
        if (sv->cur_hdr.flags & SELVA_PROTO_HDR_FLAST && sv->msg_buf_i < sv->cmd_size) {
            /*
             * Replication command not fully read and the stream was terminated abruptly.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Stream terminated abruptly");
            return REPL_PROTO_STATE_ERR;
        }

        /* Still missing frame(s) for this command. */
        sv->recv_next_frame = 1;
        /* keep the state untouched */
    }

    return REPL_PROTO_STATE_EXEC_CMD;
}

static void on_data(struct event *event, void *arg)
{
    struct replication_sock_state *sv = (struct replication_sock_state *)arg;
    const int fd = event->fd;

    while (1) {
        if (sv->recv_next_frame) {
            int err;

            err = recv_frame(sv, fd);
            if (err) {
                sv->state = REPL_PROTO_STATE_ERR;
                goto state_err;
            }
        }

        switch (sv->state) {
        case REPL_PROTO_STATE_PARSE_REPLICATION_HEADER:
            sv->state = parse_replication_header(sv);
            continue;
        case REPL_PROTO_STATE_RECEIVING_CMD:
            sv->state = handle_recv_cmd(sv);
            /* TODO Here we could also decide to return for now to allow
             * processing of other incoming messages while we are Receiving this
             * command.
             */
            continue;
        case REPL_PROTO_STATE_RECEIVING_SDB:
            /* TODO */
            return;
        case REPL_PROTO_STATE_EXEC_CMD:
            if (sv->msg_buf_i != sv->cmd_size) {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication payload size. received: %zu expected: %zu", sv->msg_buf_i, sv->cmd_size);
                sv->state = REPL_PROTO_STATE_ERR;
                continue;
            }

            SELVA_LOG(SELVA_LOGL_INFO, "Replicating %d\n", sv->cmd_id);
            selva_server_run_cmd(sv->cmd_id, sv->msg_buf, sv->cmd_size);
            sv->state = REPL_PROTO_STATE_FIN;
            continue;
        case REPL_PROTO_STATE_EXEC_SDB:
            /* TODO apply the sdb */
            sv->state = REPL_PROTO_STATE_FIN;
            continue;
state_err:
        case REPL_PROTO_STATE_ERR:
            evl_end_fd(fd);
            __attribute__((fallthrough));
        case REPL_PROTO_STATE_FIN:
            init_sv(sv);
            return;
        }
    }
}

static void on_close(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct replication_sock_state *sv = (struct replication_sock_state *)arg;

    /* TODO Should we exit or retry? */
    SELVA_LOG(SELVA_LOGL_WARN, "Replication has stopped due to connection reset");

    selva_free(sv);

    (void)shutdown(fd, SHUT_RDWR);
    close(fd);
}

int replication_replica_start(int sock)
{
    int err;
    struct replication_sock_state *sv;

    sv = selva_calloc(1, sizeof(*sv));
    init_sv(sv);

    err = send_sync_req(sock);
    if (err) {
        return err;
    }

    err = evl_wait_fd(sock, on_data, NULL, on_close, sv);
    return err;
}
