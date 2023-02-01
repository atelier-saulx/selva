/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "event_loop.h"
#include "util/finalizer.h"
#include "util/selva_string.h"
#include "selva_proto.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "origin/origin.h"
#include "replica/replicaof.h"
#include "selva_replication.h"

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
 * behaviour of these functions. For example the replication_replicate()
 * function is a NOP for a node in `replica` mode.
 */

void selva_replication_new_sdb(char sdb_hash[HASH_SIZE])
{
    if (replication_mode == REPLICATION_MODE_ORIGIN) {
        replication_origin_new_sdb(sdb_hash);
    }
}

void selva_replication_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
    if (replication_mode == REPLICATION_MODE_ORIGIN) {
        replication_origin_replicate(cmd, buf, buf_size);
    }
}

void selva_replication_stop(void)
{
    if (replication_mode == REPLICATION_MODE_ORIGIN) {
        replication_origin_stop();
        /* TODO */
    } else if (replication_mode == REPLICATION_MODE_REPLICA) {
        /* TODO */
    }
}

static void send_mode_error(struct selva_server_response_out *resp)
{
    selva_send_errorf(resp, SELVA_ENOTSUP, "Already configured as %s", replication_mode_str[replication_mode]);
}

/**
 * Start sending replication traffic to the caller.
 */
static void replicasync(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    int err;
    struct selva_server_response_out *stream_resp;

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
    }

    err = replication_origin_register_replica(stream_resp);
    if (err) {
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
 * TODO Make replica read-only
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

static void replicainfo(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    if (size) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, 3);
    selva_send_strf(resp, "%s", replication_mode_str[replication_mode]);
    selva_send_str(resp, "", 0); /* TODO hash? */
    selva_send_ll(resp, 0); /* TODO offset */
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(evl_wait_fd);
    import_selva_server();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init replication");

    SELVA_MK_COMMAND(CMD_REPLICASYNC_ID, SELVA_CMD_MODE_MUTATE, replicasync);
    SELVA_MK_COMMAND(CMD_REPLICAOF_ID, SELVA_CMD_MODE_MUTATE, replicaof);
    SELVA_MK_COMMAND(CMD_REPLICAINFO_ID, SELVA_CMD_MODE_PURE, replicainfo);
}
