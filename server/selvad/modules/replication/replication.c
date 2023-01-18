/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "selva_log.h"
#include "selva_server.h"
#include "ring_buffer.h"

/* TODO This should be same as SDB HASH_SIZE */
#define HASH_SIZE 32

#if 0
#define ISPOW2(x) ((x != 0) && ((x & (x - 1)) == 0))
static_assert(ISPOW2(CMD_LOG_SIZE));
#endif

static struct replication_state {
    char sdb_hash[HASH_SIZE];
    struct ring_buffer rb;
} replication_state;

void replication_new_sdb(char sdb_hash[HASH_SIZE])
{
}

void replication_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
}

static void replicaof(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    if (size != 2) {
        selva_send_error_arity(resp);
        return;
    }

    /* TODO IP and port */
}

static void replicainfo(struct selva_server_response_out *resp, const void *buf, size_t size)
{
    if (size != 0) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, 3);
    selva_send_strf(resp, "origin"); /* TODO origin or replica */
    selva_send_str(resp, "", 0); /* TODO hash? */
    selva_send_ll(resp, 0); /* TODO offset */
}

IMPORT() {
    evl_import_main(selva_log);
    import_selva_server();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init replication");

    SELVA_MK_COMMAND(CMD_REPLICAOF_ID, replicaof);
    SELVA_MK_COMMAND(CMD_REPLICAINFO_ID, replicainfo);
}
