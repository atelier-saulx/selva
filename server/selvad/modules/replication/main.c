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
#include "module.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "util/timestamp.h"
#include "event_loop.h"
#include "selva_proto.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "selva_replication.h"
#include "origin/replication.h"
#include "replica/replication.h"

enum replication_mode {
    REPLICATION_MODE_NONE = 0,
    REPLICATION_MODE_ORIGIN,
    REPLICATION_MODE_REPLICA,
};

static enum replication_mode replication_mode = REPLICATION_MODE_NONE;
static const char replication_mode_str[3][8] = {
    "NONE",
    "ORIGIN",
    "REPLICA",
};

/*
 * replication_new_sdb(), replication_replicate(), and replication_stop() can be
 * called in any replication mode. However, the current mode affects the
 * behaviour of these functions.
 */

void selva_replication_new_sdb(const struct selva_string *filename, const uint8_t sdb_hash[HASH_SIZE])
{
    switch (replication_mode) {
    case REPLICATION_MODE_ORIGIN:
        replication_origin_new_sdb(filename, sdb_hash);
        break;
    default:
        /* NOP */
    }
}

void selva_replication_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
    switch (replication_mode) {
    case REPLICATION_MODE_ORIGIN:
        replication_origin_replicate(cmd, buf, buf_size);
        break;
    default:
        /* NOP */
    }
}

void selva_replication_stop(void)
{
    switch (replication_mode) {
    case REPLICATION_MODE_ORIGIN:
        replication_origin_stop();
        /* TODO */
        break;
    case REPLICATION_MODE_REPLICA:
        /* TODO */
        break;
    default:
        /* NOP */
    }
}

static void send_mode_error(struct selva_server_response_out *resp)
{
    selva_send_errorf(resp, SELVA_ENOTSUP, "Already configured as %s", replication_mode_str[replication_mode]);
}

/**
 * Ensure that we have an SDB dump in the ring buffer.
 * RFE Should this be in origin/replication.c?
 */
static int ensure_sdb(void)
{
    if (replication_origin_get_last_eid()) {
        return 0;
    } else {
        struct {
            struct selva_proto_array arr_hdr;
            struct selva_proto_string str_hdr;
            char buf[20 + 1 + 3]; /*!< Filename. `[TS].sdb` */
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
        static_assert((char *)(&msg.arr_hdr) + sizeof(msg.arr_hdr) == (char *)(&msg.str_hdr));
        static_assert((char *)(&msg.str_hdr) + sizeof(msg.str_hdr) == msg.buf);

        /*
         * This happens to be the same as in replication/replication.c but
         * it's definitely not necessary.
         */
        msg.str_hdr.bsize = snprintf(msg.buf, sizeof(msg.buf), "%" PRIu64 ".sdb", (uint64_t)ts_now());
        msg_size = sizeof(msg.arr_hdr) + sizeof(msg.str_hdr) + msg.str_hdr.bsize;
        msg.str_hdr.bsize = htole16(msg.str_hdr.bsize);

        selva_server_run_cmd(CMD_SAVE_ID, &msg, msg_size);

        if (!replication_origin_get_last_eid()) {
            return SELVA_EIO;
        }
    }

    return 0;
}

static void replicainit(struct selva_server_response_out *resp, const void *buf __unused, size_t size)
{
    int err;

    if (size) {
        selva_send_error_arity(resp);
    }

    switch (replication_mode) {
    case REPLICATION_MODE_NONE:
        replication_mode = REPLICATION_MODE_ORIGIN;
        replication_origin_init();
        err = ensure_sdb();
        if (err) {
            selva_send_error(resp, err, NULL, 0);
        }
        selva_send_ll(resp, 1);
        break;
    case REPLICATION_MODE_ORIGIN:
        selva_send_ll(resp, 0);
        break;
    default:
        send_mode_error(resp);
        break;
    }
}

/**
 * Start sending replication traffic to the caller.
 */
static void replicasync(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    struct selva_server_response_out *stream_resp;
    int err;

    if (size) {
        /*
         * TODO We'd actually like to get the client's current hash and offset here
         * to speed up the sync.
         */
        selva_send_error_arity(resp);
        return;
    }

    if (replication_mode == REPLICATION_MODE_NONE) {
        replication_mode = REPLICATION_MODE_ORIGIN;
        replication_origin_init();
    } else if (replication_mode != REPLICATION_MODE_ORIGIN) {
        send_mode_error(resp);
        return;
    }

    err = selva_start_stream(resp, &stream_resp);
    if (err) {
        selva_send_errorf(resp, err, "Failed to create a stream");
        return;
    }

    err = ensure_sdb();
    if (err) {
        selva_cancel_stream(resp, stream_resp);
        selva_send_errorf(resp, err, "Failed to write an SDB dump");
        return;
    }

    /* TODO Try to use an offset id provided by the replica. */
    err = replication_origin_register_replica(stream_resp, replication_origin_get_last_eid());
    if (err) {
        selva_cancel_stream(resp, stream_resp);
        selva_send_error(resp, err, NULL, 0);
        return;
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
 * TODO Block calling replicaof for the node itself
 */
static void replicaof(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv = NULL;
    int argc;
    struct sockaddr_in origin_addr;
    int sock, err;

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

    if (replication_mode != REPLICATION_MODE_NONE) {
        send_mode_error(resp);
        return;
    }

    err = args_to_addr(&origin_addr, argv[ARGV_IP], argv[ARGV_PORT]);
    if (err) {
        selva_send_errorf(resp, err, "Invalid origin address");
        return;
    }

    sock = replication_replica_connect_to_origin(&origin_addr);
    if (sock < 0) {
        selva_send_errorf(resp, err, "Connection failed");
        return;
    }

    /* TODO Error handling */
    replication_replica_start(sock);
    replication_mode = REPLICATION_MODE_REPLICA;
    selva_server_set_readonly();

    selva_send_ll(resp, 1);
}

static void replicainfo(struct selva_server_response_out *resp, const void *buf __unused, size_t size)
{
    char hash[HASH_SIZE];

    if (size) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, 3);
    selva_send_strf(resp, "%s", replication_mode_str[replication_mode]);
    switch (replication_mode) {
    case REPLICATION_MODE_NONE:
        selva_send_null(resp);
        break;
    case REPLICATION_MODE_ORIGIN:
        selva_send_str(resp, replication_origin_get_sdb(hash), HASH_SIZE);
        break;
    case REPLICATION_MODE_REPLICA:
        selva_send_str(resp, "", 0);
    }
    selva_send_ll(resp, 0); /* TODO offset */
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
    import_selva_server();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init replication");

    SELVA_MK_COMMAND(CMD_REPLICAINIT_ID, SELVA_CMD_MODE_MUTATE, replicainit);
    SELVA_MK_COMMAND(CMD_REPLICASYNC_ID, SELVA_CMD_MODE_MUTATE, replicasync);
    SELVA_MK_COMMAND(CMD_REPLICAOF_ID, SELVA_CMD_MODE_MUTATE, replicaof);
    SELVA_MK_COMMAND(CMD_REPLICAINFO_ID, SELVA_CMD_MODE_PURE, replicainfo);
}
