/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "jemalloc.h"
#include "util/selva_string.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_replication.h"
#include "selva_server.h"
#include "../selva_thread.h"
#include "../eid.h"
#include "../sync_mode.h"
#include "ring_buffer.h"
#include "replica.h"
#include "replication.h"

#define RING_BUFFER_SIZE 100
#define MAX_REPLICAS 32

static_assert(sizeof(ring_buffer_eid_t) >= sizeof(uint64_t));

/**
 * Request replica reader threaders in the mask to stop and release their data.
 * This function will block until the threads exit.
 */
static void drop_replicas(unsigned replicas);

static struct origin_state {
    /*!<
     * Id of the lastest sdb still in rb.
     * If this is zero then a new dump must be made before starting a
     * new replica.
     */
    ring_buffer_eid_t last_sdb_eid;
    ring_buffer_eid_t last_cmd_eid; /*!< Last cmd eid. */

    struct ring_buffer rb;
    struct ring_buffer_element buffer[RING_BUFFER_SIZE];

    struct replica replicas[MAX_REPLICAS];
} origin_state;

static void insert(ring_buffer_eid_t eid, int8_t cmd, void *p, size_t p_size) {
    unsigned not_replicated;

    while ((not_replicated = ring_buffer_insert(&origin_state.rb, eid, cmd, p, p_size))) {
        drop_replicas(not_replicated);
    }
}

void replication_origin_new_sdb(const struct selva_string *filename, const uint8_t sdb_hash[SELVA_IO_HASH_SIZE])
{
    struct sdb *sdb = selva_malloc(sizeof(struct sdb));
    uint64_t sdb_eid = replication_new_origin_eid(filename);

    /* RFE Do we really need to dup here? */
    sdb->filename = selva_string_dup(filename, 0);
    memcpy(sdb->hash, sdb_hash, SELVA_IO_HASH_SIZE);

    SELVA_LOG(SELVA_LOGL_INFO, "New SDB: %s (0x%" PRIx64 ")", selva_string_to_str(filename, NULL), sdb_eid);

    insert(sdb_eid, 0, sdb, sizeof(*sdb));
    origin_state.last_sdb_eid = sdb_eid;
    origin_state.last_cmd_eid = 0;
}

uint64_t replication_origin_get_last_sdb_eid(void)
{
    return origin_state.last_sdb_eid;
}

uint64_t replication_origin_get_last_cmd_eid(void)
{
    return origin_state.last_cmd_eid;
}

static void free_replbuf(void *buf, ring_buffer_eid_t eid)
{
    if (eid & EID_MSB_MASK) {
        struct sdb *sdb = (struct sdb *)buf;

        if (eid == origin_state.last_sdb_eid) {
            origin_state.last_sdb_eid = 0;
        }

        selva_string_free(sdb->filename);
    }

    selva_free(buf);
}

/**
 * Allocate a new replica_id.
 */
static struct replica *new_replica(struct selva_server_response_out *resp)
{
    for (int i = 0; i < MAX_REPLICAS; i++) {
        if (!origin_state.replicas[i].in_use) {
            struct replica *r = &origin_state.replicas[i];

            r->in_use = 1;
            r->resp = resp;
            return r;
        }
    }

    return NULL;
}

/**
 * Release a previously allocated replica_id.
 */
static inline void release_replica(struct replica *r)
{
    r->in_use = 0;
}

static void drop_replicas(unsigned replicas)
{
    unsigned replica_id;

    ring_buffer_del_readers_mask(&origin_state.rb, replicas);
    while ((replica_id = __builtin_ffs(replicas))) {
        struct replica *r;

        replica_id--;
        assert(replica_id < num_elem(origin_state.replicas));
        r = &origin_state.replicas[replica_id];

        pthread_join(r->thread.pthread, NULL);
        release_replica(r);

        replicas ^= 1 << replica_id;
    }
}

int replication_origin_register_replica(
        struct selva_server_response_out *resp,
        uint64_t start_eid,
        const uint8_t start_sdb_hash[SELVA_IO_HASH_SIZE],
        enum replication_sync_mode mode)
{
    struct replica *replica;

    replica = new_replica(resp);
    if (!replica) {
        return SELVA_ENOBUFS;
    }

    replica->start_eid = start_eid;
    memcpy(replica->start_sdb_hash, start_sdb_hash, SELVA_IO_HASH_SIZE);
    replica->sync_mode = mode;
    ring_buffer_add_reader(&origin_state.rb, replica->id);
    pthread_create(&replica->thread.pthread, NULL, replication_thread, replica);

    return 0;
}

static ring_buffer_eid_t next_eid(void)
{
    const ring_buffer_eid_t sdb_eid_um = origin_state.last_sdb_eid & ~EID_MSB_MASK;
    ring_buffer_eid_t eid;

    if (sdb_eid_um > origin_state.last_cmd_eid) {
        origin_state.last_cmd_eid = sdb_eid_um;
    }
    eid = origin_state.last_cmd_eid = (origin_state.last_cmd_eid + 1) & ~EID_MSB_MASK;

    return eid;
}

void replication_origin_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
    /* TODO We'd really like to avoid at least malloc() here, preferrably also memcpy(). */
    void *p = selva_malloc(buf_size);

    memcpy(p, buf, buf_size);
    insert(next_eid(), cmd, p, buf_size);
}

void replication_origin_init(void)
{
    for (unsigned i = 0; i < MAX_REPLICAS; i++) {
        struct replica *r = &origin_state.replicas[i];

        r->id = i;
    /* TODO Do global core mapping */
#if 0
        r->core_id = i % nr_cores;
#endif
        r->rb = &origin_state.rb;
    }
    ring_buffer_init(&origin_state.rb, origin_state.buffer, num_elem(origin_state.buffer), free_replbuf);
}
