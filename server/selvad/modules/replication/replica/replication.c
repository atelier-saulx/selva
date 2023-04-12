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
#include "util/sdb_name.h"
#include "util/selva_string.h"
#include "util/tcp.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "../../../commands.h"
#include "../../../tunables.h"
#include "../eid.h"
#include "../replication.h"

struct replica_state {
    /**
     * Receive a new frame.
     * Set if the state machine should receive a new frame on the next cycle.
     */
    int recv_next_frame;

    /**
     * Replication state machine state.
     *
     * Legal state flows:
     * Receiving an SDB dump:
     * 1.   REPL_PROTO_STATE_PARSE_REPLICATION_HEADER
     * 2.   REPL_PROTO_STATE_RECEIVING_SDB_HEADER
     * 3.   REPL_PROTO_STATE_RECEIVING_SDB
     * n.   REPL_PROTO_STATE_EXEC_SDB
     * n+1. REPL_PROTO_STATE_FIN
     *
     * Receiving a command:
     * 1.   REPL_PROTO_STATE_PARSE_REPLICATION_HEADER
     * 2.   REPL_PROTO_STATE_RECEIVING_CMD
     * n.   REPL_PROTO_STATE_EXEC_CMD
     * n+1. REPL_PROTO_STATE_FIN
     *
     * REPL_PROTO_STATE_ERR can be the next state of any other state except
     * REPL_PROTO_STATE_FIN.
     * REPL_PROTO_STATE_FIN is the only final state of the state machine.
     */
    enum repl_proto_state {
        REPL_PROTO_STATE_PARSE_REPLICATION_HEADER,
        REPL_PROTO_STATE_RECEIVING_CMD, /*!< Receiving more frames for the replicated command. */
        REPL_PROTO_STATE_RECEIVING_SDB_HEADER,
        REPL_PROTO_STATE_RECEIVING_SDB, /*!< Receiving more chucks for an SDB dump. */
        REPL_PROTO_STATE_EXEC_CMD, /*!< Execute the received command. */
        REPL_PROTO_STATE_EXEC_SDB, /*!< Load the received SDB dump. */
        REPL_PROTO_STATE_ERR, /*!< An error occurred. The replication terminates (after FIN). */
        REPL_PROTO_STATE_FIN, /*!< Final state of receiving and replicating a message. */
    } state;

    /*
     * selva_proto frame handling.
     */
    struct selva_proto_header cur_hdr; /*!< Current header. */
    size_t cur_payload_size; /*! Size of the last received payload. */

    /*
     * Replication.
     */
    union {
        struct {
            uint64_t incoming_cmd_eid; /*!< We are currently receiving this command. */
            int64_t cmd_ts; /*!< Time stamp when the origin executed this command. */
            size_t cmd_size; /*!< Size of the incoming command in bytes. */
            int8_t cmd_id; /*!< Used if replicating a command. */
            int8_t cmd_compress;
        };
        struct {
            uint64_t incoming_sdb_eid; /*!< We are currently receiving this SDB. */
            size_t sdb_size; /*!< Size of the incoming SDB dump in bytes. */
            size_t sdb_received_bytes; /*!< Number of bytes already received. */
        };
    };

    /*
     * State used during receiving an SDB dump.
     * States using this information:
     * - REPL_PROTO_STATE_RECEIVING_SDB_HEADER,
     * - REPL_PROTO_STATE_RECEIVING_SDB,
     * - REPL_PROTO_STATE_EXEC_CMD,
     * - REPL_PROTO_STATE_EXEC_SDB,
     */
    char sdb_filename[sizeof("replica") + SDB_NAME_MIN_BUF_SIZE]; /*!< Filename. `replica-[TS].sdb` */
    FILE *sdb_file;

    size_t msg_buf_i; /*!< Index in buf. */

    /**************************************************************************
     * reinit_sv() should not touch things beyond this point.                 *
     **************************************************************************/

    struct sockaddr_in origin_addr;
    struct backoff_timeout backoff; /*!< Backoff state for reconn retries. */
    int sock;

    /*
     * Last complete transfers.
     */
    uint64_t last_sdb_eid;
    uint64_t last_cmd_eid;

    /*
     * Buffer for receiving data.
     */
    _Alignas(uintptr_t) uint8_t msg_buf[1048576];
};

/**
 * Here we store the current state of the replica replication protocol.
 */
static struct replica_state sv __lazy_alloc_glob;

int replication_running;

/**
 * When `replicaof` command is executed on a replica, we send a `replicasync`
 * command to the specified origin. At first we try to include the last good
 * (loaded) SDB hash and sdb eid (if both are known) in the request payload.
 * This is called a partial sync. If the origin recognizes the SDB then it
 * will only send commands executed (changes made) after that (sync) point;
 * If it doesn't recognize the SDB we have loaded then it will send the
 * full db dump over the socket, which we'll save and load.
 * This variable is used as a flag to mark that a partial sync has failed,
 * which we know because the origin sent an error and reset the connection.
 * Therefore, we know to send a full sync request on the next retry attempt,
 * i.e. a `replicasync` command without the hash and eid.
 */
static int partial_sync_fail;

static inline int restart_replication(struct timespec *t);

static void reinit_sv(void)
{
    /* Just clean the state variables and leave the origin_addr and buffer as is. */
    memset(&sv, 0, offsetof(struct replica_state, origin_addr));

    sv.recv_next_frame = 1;
    sv.state = REPL_PROTO_STATE_PARSE_REPLICATION_HEADER;
}

uint64_t replication_replica_get_last_sdb_eid(void)
{
    return sv.last_sdb_eid;
}

uint64_t replication_replica_get_last_cmd_eid(void)
{
    return sv.last_cmd_eid;
}

uint64_t replication_replica_new_sdb(const char *filename)
{
    uint64_t sdb_eid = replication_new_replica_sdb_eid(filename);

    if (sdb_eid) {
        sv.last_sdb_eid = sdb_eid;
        sv.last_cmd_eid = 0;

        SELVA_LOG(SELVA_LOGL_INFO, "New SDB: %s (0x%" PRIx64 ")", filename, sv.last_sdb_eid);
    }

    return sdb_eid;
}

int replication_replica_is_stale(void)
{
    return !sv.sock;
}

static int send_sync_req(int sock)
{
    const int seqno = 0;
    struct {
        struct selva_proto_header hdr;
        struct selva_proto_array arr;
        struct selva_proto_string sdb_hash_hdr;
        uint8_t sdb_hash[SELVA_IO_HASH_SIZE];
        struct selva_proto_longlong sdb_eid;
    } __attribute__((packed,aligned(__alignof__(uint64_t)))) buf = { /* TODO alignof might lead to padding */
        .hdr = {
            .cmd = CMD_ID_REPLICASYNC,
            .flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
            .seqno = htole32(seqno),
            .frame_bsize = htole16(sizeof(buf)),
            .msg_bsize = htole32(sizeof(buf) - sizeof(struct selva_proto_header)),
            .chk = 0,
        },
        .arr = {
            .type = SELVA_PROTO_ARRAY,
            .length = 2,
        },
        .sdb_hash_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = SELVA_PROTO_STRING_FBINARY,
            .bsize = SELVA_IO_HASH_SIZE,
        },
        .sdb_eid = {
            .type = SELVA_PROTO_LONGLONG,
        },
    };
    static_assert(sizeof(buf) <= SELVA_PROTO_FRAME_SIZE_MAX);

    memcpy(buf.sdb_hash, last_sdb_hash, SELVA_IO_HASH_SIZE);
    buf.sdb_eid.v = htole64(sv.last_sdb_eid);
    buf.hdr.chk = htole32(crc32c(0, &buf, sizeof(buf)));

    if (send(sock, &buf, sizeof(buf), 0) != (ssize_t)sizeof(buf)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Sending REPLICASYNC request failed");
        return SELVA_EIO;
    }

    return 0;
}

static int send_full_sync_req(int sock)
{
    const int seqno = 0;
    struct {
        struct selva_proto_header hdr;
    } __attribute__((packed,aligned(__alignof__(uint64_t)))) buf = { /* TODO alignof might lead to padding */
        .hdr = {
            .cmd = CMD_ID_REPLICASYNC,
            .flags = SELVA_PROTO_HDR_FFIRST | SELVA_PROTO_HDR_FLAST,
            .seqno = htole32(seqno),
            .frame_bsize = htole16(sizeof(buf)),
            .msg_bsize = htole32(sizeof(buf) - sizeof(struct selva_proto_header)),
            .chk = 0,
        },
    };

    buf.hdr.chk = htole32(crc32c(0, &buf, sizeof(buf)));

    if (send(sock, &buf, sizeof(buf), 0) != (ssize_t)sizeof(buf)) {
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

static int recv_frame(int fd)
{
    ssize_t recv_res;

retry_hdr:
    recv_res = tcp_recv(fd, &sv.cur_hdr, sizeof(sv.cur_hdr), 0);
    if (recv_res != (ssize_t)sizeof(sv.cur_hdr)) {
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

    if (!(sv.cur_hdr.flags & (SELVA_PROTO_HDR_FREQ_RES | SELVA_PROTO_HDR_STREAM))) {
        SELVA_LOG(SELVA_LOGL_ERR, "Unexpected frame flags: 0x%x", sv.cur_hdr.flags);
        return SELVA_PROTO_EBADMSG;
    } else if (sv.cur_hdr.cmd != CMD_ID_REPLICASYNC) {
        SELVA_LOG(SELVA_LOGL_ERR, "Unexpected command. received: %d expected: %d", sv.cur_hdr.cmd, CMD_ID_REPLICASYNC);
        return SELVA_PROTO_EBADMSG;
    }

    const size_t payload_size = le16toh(sv.cur_hdr.frame_bsize) - sizeof(sv.cur_hdr);
    if (sv.msg_buf_i + payload_size > sizeof(sv.msg_buf)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Buffer overflow");
        return SELVA_PROTO_ENOBUFS;
    } else if (payload_size > 0) {
retry_payload:
        recv_res = tcp_recv(fd, sv.msg_buf + sv.msg_buf_i, payload_size, 0);
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

    sv.cur_payload_size = payload_size;

    if (!selva_proto_verify_frame_chk(&sv.cur_hdr, sv.msg_buf + sv.msg_buf_i, payload_size)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Frame checksum mismatch");
        return SELVA_PROTO_EBADMSG;
    }

    sv.recv_next_frame = 0;
    return 0;
}

static enum repl_proto_state parse_replication_header(void)
{
    struct selva_proto_control *ctrl = (struct selva_proto_control *)sv.msg_buf;

    if (sv.cur_payload_size < sizeof(*ctrl)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Invalid payload size");
        return REPL_PROTO_STATE_ERR;
    }

    if (ctrl->type == SELVA_PROTO_REPLICATION_CMD) {
        int err, cmd_compress;

        err = selva_proto_parse_replication_cmd(sv.msg_buf, sv.cur_payload_size, 0, &sv.incoming_cmd_eid, &sv.cmd_ts, &sv.cmd_id, &cmd_compress, &sv.cmd_size);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse the replication cmd header: %s", selva_strerror(err));
            return REPL_PROTO_STATE_ERR;
        }
        sv.cmd_compress = cmd_compress;

        uint8_t *p = sv.msg_buf + sv.msg_buf_i;
        sv.cur_payload_size -= sizeof(struct selva_proto_replication_cmd);
        memmove(p, p + sizeof(struct selva_proto_replication_cmd), sv.cur_payload_size);

         return REPL_PROTO_STATE_RECEIVING_CMD;
    } else if (ctrl->type == SELVA_PROTO_REPLICATION_SDB) {
        uint64_t sdb_eid;
        size_t sdb_size;
        int err;

        err = selva_proto_parse_replication_sdb(sv.msg_buf, sv.cur_payload_size, 0, &sdb_eid, &sdb_size);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse the replication sdb header: %s", selva_strerror(err));
            return REPL_PROTO_STATE_ERR;
        }
        if (sdb_size > 0) {
            sv.incoming_sdb_eid = sdb_eid;
            sv.sdb_size = sdb_size;
            sdb_name(sv.sdb_filename, sizeof(sv.sdb_filename), "replica", sdb_eid & ~EID_MSB_MASK);

            sv.recv_next_frame = 1;
            return REPL_PROTO_STATE_RECEIVING_SDB_HEADER;
        } else {
            /*
             * We could verify whether this was a pseudo-sdb by checking the
             * SELVA_PROTO_REPLICATION_SDB_FPSEUDO flag but in either case
             * we should be able to assume that we are in sync with this
             * sdb_eid.
             */
            sv.last_sdb_eid = sdb_eid;
            sv.last_cmd_eid = 0;

            return REPL_PROTO_STATE_FIN;
        }
    } else if (ctrl->type == SELVA_PROTO_ERROR) {
        int err, origin_err;
        const char *msg_str;
        size_t msg_len;

        err = selva_proto_parse_error(sv.msg_buf, sizeof(sv.msg_buf), 0, &origin_err, &msg_str, &msg_len);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse incoming error: %s", selva_strerror(err));
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Origin error: (%s) msg: \"%.*s\"", selva_strerror(origin_err), (int)msg_len, msg_str);
            if (origin_err == SELVA_ENOENT) {
                partial_sync_fail = 1;
            }
        }
        return REPL_PROTO_STATE_ERR;
    }

    SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication header type: %s", selva_proto_type_to_str(ctrl->type, NULL));

    return REPL_PROTO_STATE_ERR;
}

static enum repl_proto_state handle_recv_cmd(void)
{
    sv.msg_buf_i += sv.cur_payload_size;

    if (sv.msg_buf_i < sv.cmd_size) {
        /*
         * Continue receiving.
         */
        if (sv.cur_hdr.flags & SELVA_PROTO_HDR_FLAST && sv.msg_buf_i < sv.cmd_size) {
            /*
             * Replication command not fully read and the stream was terminated abruptly.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "Stream terminated abruptly");
            return REPL_PROTO_STATE_ERR;
        }

        /* Still missing frame(s) for this command. */
        sv.recv_next_frame = 1;
        /* keep the state untouched */
    } else {
#if 0
            SELVA_LOG(SELVA_LOGL_DBG, "Compressed cmd: %d", sv.cmd_compress);
#endif
        if (sv.cmd_compress) {
            struct selva_string *s = selva_string_create((const char *)sv.msg_buf, sv.cmd_size, SELVA_STRING_COMPRESS);
            size_t new_len = selva_string_getz_ulen(s);

            if (new_len >= sizeof(sv.msg_buf)) {
                SELVA_LOG(SELVA_LOGL_ERR, "Replicated cmd too big");
                selva_string_free(s);
                return REPL_PROTO_STATE_ERR;
            }

#if 0
            SELVA_LOG(SELVA_LOGL_DBG, "Decompressing cmd");
#endif

            selva_string_decompress(s, (char *)sv.msg_buf);
            selva_string_free(s);
            sv.cmd_size = new_len;
        } else {
            if (sv.msg_buf_i != sv.cmd_size) {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid replication payload size. received: %zu expected: %zu", sv.msg_buf_i, sv.cmd_size);
                return REPL_PROTO_STATE_ERR;
            }
        }
    }

    return REPL_PROTO_STATE_EXEC_CMD;
}

static enum repl_proto_state handle_recv_sdb_header(void)
{
    if (sv.cur_hdr.msg_bsize != sv.sdb_size) {
        SELVA_LOG(SELVA_LOGL_ERR, "SDB size mismatch: header: %zu act: %zu", (size_t)sv.cur_hdr.msg_bsize, sv.sdb_size);
        return REPL_PROTO_STATE_ERR;
    }

    sv.sdb_file = fopen(sv.sdb_filename, "wb");
    if (!sv.sdb_file) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to open a dump file for writing: \"%s\"", sv.sdb_filename);
        return REPL_PROTO_STATE_ERR;
    }

    return REPL_PROTO_STATE_RECEIVING_SDB;
}

static enum repl_proto_state handle_recv_sdb(int fd)
{
    const size_t size = min(sv.sdb_size - sv.sdb_received_bytes, sizeof(sv.msg_buf));
    ssize_t recv_res;

retry:
    recv_res = tcp_recv(fd, sv.msg_buf, size, 0);
    if (recv_res != (ssize_t)size) {
        if (recv_res == -1) {
            int recv_err = recv_error();

            if (recv_err) {
                SELVA_LOG(SELVA_LOGL_ERR, "recv error: %s", selva_strerror(recv_err));
            } else {
                goto retry;
            }
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Invalid message size returned by tcp_recv. act: %zu exp: %zu",
                      (ssize_t)size, recv_res);
        }

        return REPL_PROTO_STATE_ERR;
    }

    (void)fwrite(sv.msg_buf, size, 1, sv.sdb_file);
    sv.sdb_received_bytes += size;

    assert(sv.sdb_received_bytes <= sv.sdb_size);
    if (sv.sdb_received_bytes == sv.sdb_size) {
        fclose(sv.sdb_file);
        sv.sdb_file = NULL;
        return REPL_PROTO_STATE_EXEC_SDB;
    } else {
        return REPL_PROTO_STATE_RECEIVING_SDB;
    }
}

static enum repl_proto_state handle_exec_sdb(void)
{
    const size_t filename_len = strnlen(sv.sdb_filename, sizeof(sv.sdb_filename));
    _Alignas(struct selva_proto_string) uint8_t buf[sizeof(struct selva_proto_string) + filename_len];
    struct selva_proto_string *ps = (struct selva_proto_string *)buf;
    int err;

    memset(ps, 0, sizeof(*ps));
    ps->type = SELVA_PROTO_STRING;
    ps->bsize = filename_len;
    memcpy(ps->data, sv.sdb_filename, filename_len);

    err = selva_server_run_cmd(CMD_ID_LOAD, 0, buf, sizeof(buf));
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to load SDB sent by the origin: %s", selva_strerror(err));
        return REPL_PROTO_STATE_ERR;
    }

    return REPL_PROTO_STATE_FIN;
}

static void on_data(struct event *event, void *arg __unused)
{
    const int fd = event->fd;

    while (1) {
        int err;

        if (sv.recv_next_frame) {
            err = recv_frame(fd);
            if (err) {
                sv.state = REPL_PROTO_STATE_ERR;
                goto state_err;
            }
        }

        switch (sv.state) {
        case REPL_PROTO_STATE_PARSE_REPLICATION_HEADER:
            sv.state = parse_replication_header();
            continue;
        case REPL_PROTO_STATE_RECEIVING_CMD:
            sv.state = handle_recv_cmd();
            if (sv.state == REPL_PROTO_STATE_RECEIVING_CMD) {
                /*
                 * Return for now to allow processing of other
                 * incoming connections.
                 */
                return;
            }
            continue;
        case REPL_PROTO_STATE_RECEIVING_SDB_HEADER:
            sv.state = handle_recv_sdb_header();
            continue;
        case REPL_PROTO_STATE_RECEIVING_SDB:
            sv.state = handle_recv_sdb(fd);
            if (sv.state == REPL_PROTO_STATE_RECEIVING_SDB) {
                /*
                 * Return for now to allow processing of other
                 * incoming connections.
                 */
                return;
            }
            continue;
        case REPL_PROTO_STATE_EXEC_CMD:
#if 0
            SELVA_LOG(SELVA_LOGL_INFO, "Replicating cmd: %d\n", sv.cmd_id);
#endif
            err = selva_server_run_cmd(sv.cmd_id, sv.cmd_ts, sv.msg_buf, sv.cmd_size);
            if (err) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to replicate a command: %s", selva_strerror(err));
            }

            sv.last_cmd_eid = sv.incoming_cmd_eid;
            sv.incoming_cmd_eid = 0;
            sv.state = err ? REPL_PROTO_STATE_ERR : REPL_PROTO_STATE_FIN;
            continue;
        case REPL_PROTO_STATE_EXEC_SDB:
            sv.state = handle_exec_sdb();
            if (sv.state == REPL_PROTO_STATE_FIN) {
                sv.last_sdb_eid = sv.incoming_sdb_eid;
                sv.last_cmd_eid = 0;
                sv.incoming_sdb_eid = 0;
            }
            continue;
state_err:
        case REPL_PROTO_STATE_ERR:
            SELVA_LOG(SELVA_LOGL_WARN, "Closing connection to origin");
            evl_end_fd(fd);
            [[fallthrough]];
        case REPL_PROTO_STATE_FIN:
            if (sv.sdb_file) {
                fclose(sv.sdb_file);
            }
            reinit_sv();
            return;
        }
    }
}

static void on_close(struct event *event, void *arg __unused)
{
    const int fd = event->fd;
    struct timespec t_retry;
    int err;

    (void)shutdown(fd, SHUT_RDWR);
    close(fd);
    sv.sock = 0;

    SELVA_LOG(SELVA_LOGL_WARN, "Replication has stopped due to connection reset");

    backoff_timeout_next(&sv.backoff, &t_retry);
    err = restart_replication(&t_retry);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to schedule a replication retry: %s", selva_strerror(err));
        exit(EXIT_FAILURE);
    }
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

static int start_replication_handler(void)
{
    int sock, err;

    reinit_sv();

    sock = connect_to_origin(&sv.origin_addr);
    if (sock < 0) {
        return sock;
    }

    if (sv.last_sdb_eid && sv.last_cmd_eid == 0 && !partial_sync_fail) {
        SELVA_LOG(SELVA_LOGL_INFO, "Partial sync req\n");
        err = send_sync_req(sock);
    } else {
        SELVA_LOG(SELVA_LOGL_INFO, "Full sync req\n");
        err = send_full_sync_req(sock);
        partial_sync_fail = 0;
    }
    if (!err) {
        err = evl_wait_fd(sock, on_data, NULL, on_close, NULL);
    }
    if (err) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    } else {
        sv.sock = sock;
    }

    return err;
}

static inline double ts2sec(struct timespec *ts)
{
    return (double)ts->tv_sec + ts->tv_nsec / 1e9;
}

static void start_replication_trampoline(struct event *, void *arg __unused)
{
    int err;

    err = start_replication_handler();
    if (err) {
        struct timespec t_retry;

        backoff_timeout_next(&sv.backoff, &t_retry);

        SELVA_LOG(SELVA_LOGL_WARN, "Connection to origin failed. Retrying in %.2F s", ts2sec(&t_retry));

        /* Retry again later. */
        restart_replication(&t_retry);
    } else {
        sv.backoff.attempt = 0;
    }
}

static inline int restart_replication(struct timespec *t)
{
    int tim;

    tim = evl_set_timeout(t, start_replication_trampoline, NULL);
    if (tim < 0) {
        return tim;
    }

    return 0;
}

int replication_replica_start(struct sockaddr_in *origin_addr)
{
    memcpy(&sv.origin_addr, origin_addr, sizeof(sv.origin_addr));
    sv.backoff = backoff_timeout_defaults;
    backoff_timeout_init(&sv.backoff);

    if (sv.sock) {
        SELVA_LOG(SELVA_LOGL_INFO, "Close old replication connection");
        shutdown(sv.sock, SHUT_RDWR);
    }

    if (!replication_running) {
        replication_running = 1;
        return restart_replication(&(struct timespec){0});
    }

    return 0;
}

void replication_replica_init(void)
{
    memset(&sv, 0, sizeof(sv));
}
