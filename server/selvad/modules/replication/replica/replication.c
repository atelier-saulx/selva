/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "endian.h"
#include "jemalloc.h"
#include "util/backoff_timeout.h"
#include "util/crc32c.h"
#include "util/tcp.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "../../../commands.h"
#include "../../../tunables.h"
#include "replication.h"

struct replication_sock_state {
    int recv_next_frame;
    enum repl_proto_state {
        REPL_PROTO_STATE_PARSE_REPLICATION_HEADER,
        REPL_PROTO_STATE_RECEIVING_CMD, /*!< Receiving more frames for the replicated command. */
        REPL_PROTO_STATE_RECEIVING_SDB_HEADER,
        REPL_PROTO_STATE_RECEIVING_SDB,
        REPL_PROTO_STATE_EXEC_CMD,
        REPL_PROTO_STATE_EXEC_SDB,
        REPL_PROTO_STATE_ERR,
        REPL_PROTO_STATE_FIN,
    } state;

    /*
     * selva_proto frame handling.
     */
    struct selva_proto_header cur_hdr;
    size_t cur_payload_size; /*! Size of the last received payload. */

    /*
     * Replication.
     */
    union {
        struct {
            int8_t cmd_id; /*!< Used if replicating a command. */
            size_t cmd_size; /*!< Size of the incoming command in bytes. */
        };
        struct {
            size_t sdb_size; /*!< Size of the incoming SDB dump in bytes. */
            size_t sdb_received_bytes; /*!< Number of bytes already received. */
        };
    };
    char sdb_filename[sizeof("replica-") + 20 + sizeof(".sdb")]; /*!< Filename. `replica-[TS].sdb` */
    FILE *sdb_file;

    size_t msg_buf_i; /*!< Index in buf. */

    /**************************************************************************
     * reinit_sv() should not touch things beyond this point.                 *
     **************************************************************************/

    struct sockaddr_in origin_addr;
    struct backoff_timeout backoff; /*!< Backoff state for reconn retries. */

    /*
     * Buffer for receiving data.
     */
    _Alignas(uintptr_t) uint8_t msg_buf[1048576];
};

extern void set_replica_stale(int s);
static inline int restart_replication(struct replication_sock_state *sv, struct timespec *t);

static void reinit_sv(struct replication_sock_state *sv)
{
    /* Just clean the state variables and leave the origin_addr and buffer as is. */
    memset(sv, 0, offsetof(struct replication_sock_state, origin_addr));

    sv->recv_next_frame = 1;
    sv->state = REPL_PROTO_STATE_PARSE_REPLICATION_HEADER;
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

static int recv_error(void) {
    switch (errno) {
        case EINTR:
            return 0;
        case EBADF:
            return SELVA_PROTO_EBADF;
        case ENOTCONN:
        case ECONNREFUSED:
            return SELVA_PROTO_ENOTCONN;
        default:
            return SELVA_PROTO_EINVAL;
    }
}

static int recv_frame(struct replication_sock_state *sv, int fd)
{
    ssize_t recv_res;

retry_hdr:
    recv_res = recv(fd, &sv->cur_hdr, sizeof(sv->cur_hdr), 0);
    if (recv_res != (ssize_t)sizeof(sv->cur_hdr)) {
        int err = SELVA_PROTO_EBADF;

        if (recv_res == -1) {
            err = recv_error();
            if (!err) {
                goto retry_hdr;
            }
        }

        SELVA_LOG(SELVA_LOGL_ERR, "Failed to receive selva_proto_header");
        return err;
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
        SELVA_LOG(SELVA_LOGL_ERR, "Buffer overflow");
        return SELVA_PROTO_ENOBUFS;
    } else if (payload_size > 0) {
retry_payload:
        recv_res = recv(fd, sv->msg_buf + sv->msg_buf_i, payload_size, 0);
        if (recv_res != (ssize_t)payload_size) {
            int err = SELVA_PROTO_EBADF;

            if (recv_res == -1) {
                err = recv_error();
                if (!err) {
                    goto retry_payload;
                }
            }

            SELVA_LOG(SELVA_LOGL_ERR, "Failed to receive a payload");
            return err;
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
        uint64_t sdb_eid;

        selva_proto_parse_replication_sdb(sv->msg_buf, sizeof(struct selva_proto_replication_sdb), 0, &sdb_eid, &sv->sdb_size);
        snprintf(sv->sdb_filename, sizeof(sv->sdb_filename), "replica-%" PRIu64 ".sdb", sdb_eid & ~EID_MSB_MASK);

        sv->recv_next_frame = 1;
        return REPL_PROTO_STATE_RECEIVING_SDB_HEADER;
    } else if (ctrl->type == SELVA_PROTO_ERROR) {
        int err, origin_err;
        const char *msg_str;
        size_t msg_len;

        err = selva_proto_parse_error(sv->msg_buf, sizeof(sv->msg_buf), 0, &origin_err, &msg_str, &msg_len);
        if (!err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Origin error: (%s) msg: \"%.*s\"", selva_strerror(origin_err), (int)msg_len, msg_str);
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse incoming error: %s", selva_strerror(err));
        }
        return REPL_PROTO_STATE_ERR;
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

static enum repl_proto_state handle_recv_sdb_header(struct replication_sock_state *sv)
{
    if (sv->cur_hdr.msg_bsize != sv->sdb_size) {
        SELVA_LOG(SELVA_LOGL_ERR, "SDB size mismatch: header: %zu act: %zu", (size_t)sv->cur_hdr.msg_bsize, sv->sdb_size);
        return REPL_PROTO_STATE_ERR;
    }

    sv->sdb_file = fopen(sv->sdb_filename, "wbx");
    if (!sv->sdb_file) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to open a dump file for writing: \"%s\"", sv->sdb_filename);
        return REPL_PROTO_STATE_ERR;
    }

    return REPL_PROTO_STATE_RECEIVING_SDB;
}

static enum repl_proto_state handle_recv_sdb(struct replication_sock_state *sv, int fd)
{
    const size_t size = min(sv->sdb_size - sv->sdb_received_bytes, sizeof(sv->msg_buf));
    ssize_t recv_res;

retry:
    recv_res = recv(fd, sv->msg_buf, size, 0);
    if (recv_res != (ssize_t)size) {
        if (recv_res == - 1) {
            if (!recv_error()) {
                goto retry;
            }
        }

        return REPL_PROTO_STATE_ERR;
    }

    (void)fwrite(sv->msg_buf, size, 1, sv->sdb_file);
    sv->sdb_received_bytes += size;

    assert(sv->sdb_received_bytes <= sv->sdb_size);
    if (sv->sdb_received_bytes == sv->sdb_size) {
        fclose(sv->sdb_file);
        sv->sdb_file = NULL;
        return REPL_PROTO_STATE_EXEC_SDB;
    } else {
        return REPL_PROTO_STATE_RECEIVING_SDB;
    }
}

static enum repl_proto_state handle_exec_sdb(struct replication_sock_state *sv)
{
    const size_t filename_len = strnlen(sv->sdb_filename, sizeof(sv->sdb_filename));
    _Alignas(struct selva_proto_string) uint8_t buf[sizeof(struct selva_proto_string) + filename_len];
    struct selva_proto_string *ps = (struct selva_proto_string *)buf;

    memset(ps, 0, sizeof(*ps));
    ps->type = SELVA_PROTO_STRING;
    ps->bsize = filename_len;
    memcpy(ps->data, sv->sdb_filename, filename_len);

    selva_server_run_cmd(CMD_LOAD_ID, buf, sizeof(buf));

    return REPL_PROTO_STATE_FIN;
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
            if (sv->state == REPL_PROTO_STATE_RECEIVING_CMD) {
                /*
                 * Return for now to allow processing of other
                 * incoming connections.
                 */
                return;
            }
            continue;
        case REPL_PROTO_STATE_RECEIVING_SDB_HEADER:
            sv->state = handle_recv_sdb_header(sv);
            continue;
        case REPL_PROTO_STATE_RECEIVING_SDB:
            sv->state = handle_recv_sdb(sv, fd);
            if (sv->state == REPL_PROTO_STATE_RECEIVING_SDB) {
                /*
                 * Return for now to allow processing of other
                 * incoming connections.
                 */
                return;
            }
            continue;
        case REPL_PROTO_STATE_EXEC_CMD:
            if (sv->msg_buf_i != sv->cmd_size) {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication payload size. received: %zu expected: %zu", sv->msg_buf_i, sv->cmd_size);
                sv->state = REPL_PROTO_STATE_ERR;
                continue;
            }

            SELVA_LOG(SELVA_LOGL_INFO, "Replicating cmd: %d\n", sv->cmd_id);
            selva_server_run_cmd(sv->cmd_id, sv->msg_buf, sv->cmd_size);
            sv->state = REPL_PROTO_STATE_FIN;
            continue;
        case REPL_PROTO_STATE_EXEC_SDB:
            sv->state = handle_exec_sdb(sv);
            continue;
state_err:
        case REPL_PROTO_STATE_ERR:
            SELVA_LOG(SELVA_LOGL_WARN, "Closing connection to origin");
            evl_end_fd(fd);
            __attribute__((fallthrough));
        case REPL_PROTO_STATE_FIN:
            if (sv->sdb_file) {
                fclose(sv->sdb_file);
            }
            reinit_sv(sv);
            return;
        }
    }
}

static void on_close(struct event *event, void *arg)
{
    const int fd = event->fd;
    struct replication_sock_state *sv = (struct replication_sock_state *)arg;
    const int retry = 1;

    SELVA_LOG(SELVA_LOGL_WARN, "Replication has stopped due to connection reset");
    set_replica_stale(1);

    (void)shutdown(fd, SHUT_RDWR);
    close(fd);
    if (retry) {
        struct timespec t_retry;
        int err;

        backoff_timeout_next(&sv->backoff, &t_retry);
        err = restart_replication(sv, &t_retry);
        if (!err) {
            return;
        }
        /* TODO LOG error */
    }

    selva_free(sv);
    exit(1);
}

/**
 * Connect to origin server for replication.
 * @returns a socket to the origin.
 */
[[nodiscard]]
static int connect_to_origin(struct sockaddr_in *origin_addr)
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

static int start_replication_handler(struct replication_sock_state *sv)
{
    int sock, err;

    reinit_sv(sv);

    sock = connect_to_origin(&sv->origin_addr);
    if (sock < 0) {
        return sock;
    }

    err = send_sync_req(sock);
    if (!err) {
        err = evl_wait_fd(sock, on_data, NULL, on_close, sv);
    }
    if (err) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }

    return err;
}

static inline double ts2sec(struct timespec *ts)
{
    return (double)ts->tv_sec + ts->tv_nsec / 1e9;
}

static void start_replication_trampoline(struct event *, void *arg)
{
    struct replication_sock_state *sv = (struct replication_sock_state *)arg;
    int err;

    err = start_replication_handler(sv);
    if (err) {
        struct timespec t_retry;

        backoff_timeout_next(&sv->backoff, &t_retry);

        SELVA_LOG(SELVA_LOGL_WARN, "Connection to origin failed. Retrying in %.2F s", ts2sec(&t_retry));

        /* Retry again later. */
        restart_replication(sv, &t_retry);
    } else {
        sv->backoff.attempt = 0;
        set_replica_stale(0);
    }
}

static inline int restart_replication(struct replication_sock_state *sv, struct timespec *t)
{
    int tim;

    tim = evl_set_timeout(t, start_replication_trampoline, sv);
    if (tim < 0) {
        return tim;
    }

    return 0;
}

int replication_replica_start(struct sockaddr_in *origin_addr)
{
    struct replication_sock_state *sv;

    sv = selva_calloc(1, sizeof(*sv));
    memcpy(&sv->origin_addr, origin_addr, sizeof(sv->origin_addr));
    sv->backoff = backoff_timeout_defaults;
    backoff_timeout_init(&sv->backoff);
    set_replica_stale(1);

    return restart_replication(sv, &(struct timespec){0});
}
