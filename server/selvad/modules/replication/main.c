/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "util/finalizer.h"
#include "util/net.h"
#include "util/sdb_name.h"
#include "util/selva_string.h"
#include "util/timestamp.h"
#include "endian.h"
#include "module.h"
#include "config.h"
#include "event_loop.h"
#include "selva_proto.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_replication.h"
#include "replication.h"

#define XSTR(s) STR(s)
#define STR(s) #s

static enum replication_mode replication_mode = SELVA_REPLICATION_MODE_NONE;
static const char replication_mode_str[4][2 * sizeof(size_t)] = {
    "NONE",
    "ORIGIN",
    "REPLICA",
    "REPLICA_STALE"
};
static int auto_save_interval;
uint8_t last_sdb_hash[SELVA_IO_HASH_SIZE];

static const struct config cfg_map[] = {
    { "SELVA_REPLICATION_MODE", CONFIG_INT, &replication_mode },
    { "AUTO_SAVE_INTERVAL",     CONFIG_INT, &auto_save_interval },
};

static const struct timespec replicawait_interval = {
    .tv_sec = 1,
    .tv_nsec = 0,
};

struct replicawait_arg {
    uint64_t eid;
    struct selva_server_response_out *stream_resp;
};

static void *new_struct(size_t size, void *v)
{
    void *p = malloc(size);

    memcpy(p, v, size);

    return p;
}

#define NEW_STRUCT(_value_) \
    (typeof(_value_))new_struct(sizeof(*(_value_)), (_value_))

enum replication_mode selva_replication_get_mode(void)
{
    return replication_mode;
}

/*
 * replication_new_sdb() and replication_replicate() can be
 * called in any replication mode. However, the current mode affects the
 * behaviour of these functions.
 */

void selva_replication_new_sdb(const char *filename, const uint8_t sdb_hash[SELVA_IO_HASH_SIZE])
{
    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        memcpy(last_sdb_hash, sdb_hash, SELVA_IO_HASH_SIZE);
        replication_origin_new_sdb(filename, last_sdb_hash);
        break;
    case SELVA_REPLICATION_MODE_REPLICA:
        memcpy(last_sdb_hash, sdb_hash, SELVA_IO_HASH_SIZE);
        (void)replication_replica_new_sdb(filename);
        break;
    default:
        ; /* NOP */
    }
}

uint64_t selva_replication_incomplete_sdb(const char *filename)
{
    uint64_t sdb_eid = 0;

    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        sdb_eid = replication_origin_new_incomplete_sdb(filename);
        break;
    case SELVA_REPLICATION_MODE_REPLICA:
        sdb_eid = replication_replica_new_sdb(filename);
        break;
    default:
        ; /* NOP */
    }

    return sdb_eid;
}

void selva_replication_complete_sdb(uint64_t sdb_eid, uint8_t sdb_hash[SELVA_IO_HASH_SIZE])
{
    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        memcpy(last_sdb_hash, sdb_hash, SELVA_IO_HASH_SIZE);
        replication_origin_complete_sdb(sdb_eid, sdb_hash);
        break;
    case SELVA_REPLICATION_MODE_REPLICA:
        memcpy(last_sdb_hash, sdb_hash, SELVA_IO_HASH_SIZE);
        break;
    default:
        ; /* NOP */
    }
}

void selva_replication_replicate(int64_t ts, int8_t cmd, const void *buf, size_t buf_size)
{
    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        replication_origin_replicate(ts, cmd, buf, buf_size);
        break;
    default:
        ; /* NOP */
    }
}

void selva_replication_replicate_pass(int64_t ts, int8_t cmd, void *buf, size_t buf_size)
{
    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        replication_origin_replicate_pass(ts, cmd, buf, buf_size);
        break;
    default:
        ; /* NOP */
    }
}

static void send_mode_error(struct selva_server_response_out *resp)
{
    selva_send_errorf(resp, SELVA_ENOTSUP, "Already configured as %s", replication_mode_str[replication_mode]);
}

static void req_dump(void)
{
    struct {
        struct selva_proto_array arr_hdr;
        struct selva_proto_string str_hdr;
        char buf[SDB_NAME_MIN_BUF_SIZE]; /*!< Filename. */
    } msg = {
        .arr_hdr = {
            .type = SELVA_PROTO_ARRAY,
            .length = 1,
        },
        .str_hdr = {
            .type = SELVA_PROTO_STRING,
            .flags = 0,
        },
    };
    size_t msg_size;

    /* Check that it's packed properly. */
#if !defined(__clang__)
    static_assert((char *)(&msg.arr_hdr) + sizeof(msg.arr_hdr) == (char *)(&msg.str_hdr));
    static_assert((char *)(&msg.str_hdr) + sizeof(msg.str_hdr) == msg.buf);
#endif

    /*
     * This happens to be the almost the same as in
     * replication/replication.c but it's definitely not
     * required.
     */
    msg.str_hdr.bsize = sdb_name(msg.buf, sizeof(msg.buf), NULL, (uint64_t)ts_monorealtime_now());
    msg_size = sizeof(msg.arr_hdr) + sizeof(msg.str_hdr) + msg.str_hdr.bsize;
    msg.str_hdr.bsize = htole16(msg.str_hdr.bsize);

    selva_server_run_cmd(CMD_ID_SAVE, ts_now(), &msg, msg_size);
}

/**
 * Ensure that we have an SDB dump in the ring buffer.
 * RFE There is still a race if we'd write new data before the replication
 *     thread registers itself.
 */
static int ensure_sdb(void)
{
    uint64_t eid;

    eid = replication_origin_get_last_sdb_eid();
    if (!eid || replication_origin_check_sdb(eid)) {
        req_dump();

        /*
         * Let the replica retry later.
         * This is much easier than trying to wait async for the dump or error
         * to appear.
         */
        return SELVA_EINPROGRESS;
    }

    return 0;
}

/**
 * Start sending replication traffic to the caller.
 */
static void replicasync(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    struct selva_server_response_out *stream_resp;
    const uint8_t *sdb_hash = NULL;
    size_t sdb_hash_len = 0;
    uint64_t sdb_eid;
    enum replication_sync_mode sync_mode;
    int argc, err;

    argc = selva_proto_scanf(NULL, buf, size, "{%.*s, %" PRIu64 "}", &sdb_hash_len, &sdb_hash, &sdb_eid);
    if (argc < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse: %s", selva_strerror(argc));
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    }

    /* selva_proto_scanf() should never let us here if this is not true. */
    assert(argc == 0 || argc == 2);

    if (replication_mode != SELVA_REPLICATION_MODE_ORIGIN) {
        send_mode_error(resp);
        return;
    }

    err = selva_start_stream(resp, &stream_resp);
    if (err) {
        selva_send_errorf(resp, err, "Failed to create a stream");
        return;
    }

    if (argc) {
        /*
         * Try to start from a save point provided by the replica and
         * do a partial sync.
         */
        sync_mode = REPLICATION_SYNC_MODE_PARTIAL;

        if (!sdb_hash || sdb_hash_len != SELVA_IO_HASH_SIZE) {
            selva_send_errorf(resp, SELVA_EINVAL, "Invalid sdb_hash length");
            return;
        }
    } else {
        /*
         * Start from whatever dump we know and do a full sync.
         */
        sync_mode = REPLICATION_SYNC_MODE_FULL;

        err = ensure_sdb();
        if (err) {
            selva_cancel_stream(resp, stream_resp);
            selva_send_errorf(resp, err, "Failed to write an SDB dump");
            return;
        }

        sdb_eid = replication_origin_get_last_sdb_eid();
        sdb_hash = last_sdb_hash;
    }

    err = replication_origin_register_replica(stream_resp, sdb_eid, sdb_hash, sync_mode);
    if (err) {
        selva_cancel_stream(resp, stream_resp);
        selva_send_errorf(resp, err, "Failed to register the replica");
    }
}

static int args_to_addr(struct sockaddr_in *addr, struct selva_string *ip, struct selva_string *port)
{
    long long port_ll;
    int err;

    err = selva_string_to_ll(port, &port_ll);
    if (err) {
        return err;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port_ll);

    if (inet_pton(AF_INET, selva_string_to_str(ip, NULL), &addr->sin_addr) == -1) {
        return SELVA_EINVAL;
    }

    return 0;
}

/**
 * Make this node a replica of an origin.
 */
static void replicaof(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv = NULL;
    struct sockaddr_in origin_addr;
    int argc, err;

    finalizer_init(&fin);

    const size_t ARGV_IP = 0;
    const size_t ARGV_PORT = 1;

    argc = selva_proto_buf2strings(&fin, buf, size, &argv);
    if (argc != 2) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    if (replication_mode != SELVA_REPLICATION_MODE_REPLICA) {
        send_mode_error(resp);
        return;
    }

    err = args_to_addr(&origin_addr, argv[ARGV_IP], argv[ARGV_PORT]);
    if (err) {
        selva_send_errorf(resp, err, "Invalid origin address");
        return;
    }

    err = replication_replica_start(&origin_addr);
    if (err) {
        selva_send_errorf(resp, err, "Connection failed");
        return;
    }

    selva_send_ll(resp, 1);
}

static void replicainfo_none(struct selva_server_response_out *resp)
{
    /*
     * - mode
     */
    selva_send_array(resp, 1);
    selva_send_strf(resp, "%s", replication_mode_str[replication_mode]);
}

static void replicainfo_origin(struct selva_server_response_out *resp)
{
    const unsigned replicas_mask = replication_origin_get_replicas_mask();
    const int nr_replicas = __builtin_popcount(replicas_mask);

    /*
     * - mode
     * - sdb_hash
     * - sdb_eid
     * - cmd_eid
     * - [[replica_id, addr, last_ack], ...]
     */
    selva_send_array(resp, 5);
    selva_send_strf(resp, "%s", replication_mode_str[replication_mode]);
    selva_send_bin(resp, last_sdb_hash, SELVA_IO_HASH_SIZE);
    selva_send_llx(resp, (long long)replication_origin_get_last_sdb_eid());
    selva_send_llx(resp, (long long)replication_origin_get_last_cmd_eid());

    selva_send_array(resp, nr_replicas);
    for (unsigned replica_id = 0; replica_id < REPLICATION_MAX_REPLICAS; replica_id++) {
        if (replicas_mask & (1 << replica_id)) {
            const struct selva_server_response_out *resp_replica;
            char buf[CONN_STR_LEN];

            resp_replica = replication_origin_get_replica_resp(replica_id);

            selva_send_array(resp, 3);
            selva_send_ll(resp, replica_id);
            selva_send_str(resp, buf, (int)selva_resp_to_str(resp_replica, buf, sizeof(buf)));
            selva_send_llx(resp, (long long)replication_origin_get_replica_last_ack(replica_id));
        }
    }
}

static void replicainfo_replica(struct selva_server_response_out *resp)
{
    char buf[CONN_STR_LEN];

    /*
     * - mode
     * - sdb_hash
     * - sdb_eid
     * - cmd_eid
     * - origin
     */
    selva_send_array(resp, 5);
    selva_send_strf(resp, "%s_%s", replication_mode_str[replication_mode], replication_replica_is_stale() ? "STALE" : "ACTIVE");
    selva_send_bin(resp, last_sdb_hash, SELVA_IO_HASH_SIZE);
    selva_send_llx(resp, (long long)replication_replica_get_last_sdb_eid());
    selva_send_llx(resp, (long long)replication_replica_get_last_cmd_eid());
    selva_send_str(resp, buf, (int)replication_replica_origin2str(buf));
}

static void replicainfo(struct selva_server_response_out *resp, const void *buf __unused, size_t size)
{
    if (size) {
        selva_send_error_arity(resp);
        return;
    }

    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_NONE:
        replicainfo_none(resp);
        break;
    case SELVA_REPLICATION_MODE_ORIGIN:
        replicainfo_origin(resp);
        break;
    case SELVA_REPLICATION_MODE_REPLICA:
        replicainfo_replica(resp);
        break;
    }
}

static void replicastatus(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    uint64_t eid;
    int argc, err;
    unsigned replication_id;

    if (replication_mode != SELVA_REPLICATION_MODE_ORIGIN) {
        selva_send_errorf(resp, SELVA_ENOTSUP, "Not an origin");
        return;
    }

    argc = selva_proto_scanf(NULL, buf, size, "%" PRIu64, &eid);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Invalid arguments");
        return;
    } else if (argc != 1) {
        selva_send_error_arity(resp);
        return;
    }

    err = replication_origin_find_replica(resp, &replication_id);
    if (err) {
        selva_send_errorf(resp, err, "replica lookup failed");
        return;
    }

    replication_origin_update_replica_last_ack(replication_id, eid);

#if 0
    SELVA_LOG(SELVA_LOGL_INFO, "replica at %" PRIu64, eid);
#endif
}

static void replicawait_cb(struct event *, void *arg)
{
    struct replicawait_arg *args = (struct replicawait_arg *)arg;
    const unsigned replicas_mask = replication_origin_get_replicas_mask();
    const int nr_replicas = __builtin_popcount(replicas_mask);
    const uint64_t expected_eid = args->eid & ~EID_MSB_MASK;
    int i = 0;

    for (unsigned replica_id = 0; replica_id < REPLICATION_MAX_REPLICAS; replica_id++) {
        if (replicas_mask & (1 << replica_id)) {
            const uint64_t last_ack = replication_origin_get_replica_last_ack(replica_id) & ~EID_MSB_MASK;

            i += last_ack >= expected_eid;
        }
    }

#if 0
    SELVA_LOG(SELVA_LOGL_INFO, "last_eid: %" PRIx64 ", %d of %d in sync", args->eid, i, nr_replicas);
#endif
    if (i == nr_replicas) {
        selva_send_ll(args->stream_resp, 1);
        selva_send_end(args->stream_resp);
        free(args);
    } else {
        evl_set_timeout(&replicawait_interval, replicawait_cb, arg);
    }
}

static void replicawait(struct selva_server_response_out *resp, const void *buf __unused, size_t size)
{
    struct selva_server_response_out *stream_resp;
    int err;

    if (size) {
        selva_send_error_arity(resp);
        return;
    }

    if (replication_mode != SELVA_REPLICATION_MODE_ORIGIN) {
        selva_send_errorf(resp, SELVA_ENOTSUP, "Not an origin");
        return;
    }

    err = selva_start_stream(resp, &stream_resp);
    if (err && err != SELVA_PROTO_ENOTCONN) {
        selva_send_errorf(resp, err, "Failed to create a stream");
        return;
    }

    evl_set_timeout(&replicawait_interval, replicawait_cb, NEW_STRUCT((&(struct replicawait_arg){
        .eid = replication_origin_get_last_eid(),
        .stream_resp = stream_resp,
    })));
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(config_resolve);
    evl_import_main(evl_set_timeout);
    evl_import_event_loop();
    import_selva_server();
}

__constructor void init(void)
{
    evl_module_init("replication");

	int err = config_resolve("replication", cfg_map, num_elem(cfg_map));
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to parse config args: %s",
                  selva_strerror(err));
        exit(EXIT_FAILURE);
    }

    if (replication_mode > 2) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Invalid replication mode");
        exit(EXIT_FAILURE);
    }

    switch (replication_mode) {
    case SELVA_REPLICATION_MODE_ORIGIN:
        if (!auto_save_interval) {
            SELVA_LOG(SELVA_LOGL_INFO, "\"AUTO_SAVE_INTERVAL\" is recommended with the \"%s\" replication mode", replication_mode_str[replication_mode]);
        }

        replication_origin_init();
        break;
    case SELVA_REPLICATION_MODE_REPLICA:
        if (auto_save_interval) {
            SELVA_LOG(SELVA_LOGL_CRIT, "The replication mode \"%s\" and \"AUTO_SAVE_INTERVAL\" are mutually exclusive", replication_mode_str[replication_mode]);
            exit(EXIT_FAILURE);
        }

        replication_replica_init();
        selva_server_set_readonly();
        break;
    default:
        ; /* NOP */
    }

    SELVA_MK_COMMAND(CMD_ID_REPLICASYNC, SELVA_CMD_MODE_MUTATE, replicasync);
    SELVA_MK_COMMAND(CMD_ID_REPLICAOF, SELVA_CMD_MODE_PURE, replicaof);
    SELVA_MK_COMMAND(CMD_ID_REPLICAINFO, SELVA_CMD_MODE_PURE, replicainfo);
    SELVA_MK_COMMAND(CMD_ID_REPLICASTATUS, SELVA_CMD_MODE_MUTATE, replicastatus);
    SELVA_MK_COMMAND(CMD_ID_REPLICAWAIT, SELVA_CMD_MODE_PURE, replicawait);
}
