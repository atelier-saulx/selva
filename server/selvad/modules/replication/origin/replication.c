/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysinfo.h>
#include "jemalloc.h"
#include "selva_error.h"
#include "selva_log.h"
#include "ring_buffer.h"
#include "replica.h"
#include "origin.h"

/* TODO public functions should be NOP if not origin */

#define RING_BUFFER_SIZE 100
#define MAX_REPLICAS 32

#define EID_MSB_MASK (~(~(typeof(replication_state.sdb_eid))0 >> 1))

static struct replication_state {
    char sdb_hash[HASH_SIZE]; /*!< The hash of the latest dump. */
    ring_buffer_eid_t sdb_eid; /*!< Id of the sdb in rb. */
    struct ring_buffer rb;
    struct ring_buffer_element buffer[RING_BUFFER_SIZE];
    struct replica replicas[MAX_REPLICAS];
} replication_state;

static ring_buffer_eid_t gen_sdb_eid(char sdb_hash[HASH_SIZE])
{
    /* FIXME generate and mark with MSB? */
    return EID_MSB_MASK;
}

void replication_origin_new_sdb(char sdb_hash[HASH_SIZE])
{
    memcpy(replication_state.sdb_hash, sdb_hash, HASH_SIZE);
    replication_state.sdb_eid = gen_sdb_eid(sdb_hash);
    /* TODO This needs more context */
    ring_buffer_insert(&replication_state.rb, replication_state.sdb_eid, replication_state.sdb_hash, HASH_SIZE);
}

static void free_replbuf(void *buf, ring_buffer_eid_t eid)
{
    if (!(eid & EID_MSB_MASK)) {
        selva_free(buf);
    }
}

/**
 * Allocate a new replica_id.
 */
static struct replica *new_replica(struct selva_server_response_out *resp)
{
    for (int i = 0; i < MAX_REPLICAS; i++) {
        if (!replication_state.replicas[i].in_use) {
            struct replica *r = &replication_state.replicas[i];

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

int replication_origin_register_replica(struct selva_server_response_out *resp)
{
    struct replica *replica = new_replica(resp);

    if (!replica) {
        return SELVA_ENOBUFS;
    }

    ring_buffer_add_reader(&replication_state.rb, replica->id);
    pthread_create(&replica->thread, NULL, replication_thread, replica);

    return 0;
}

/**
 * Request replica reader threaders in the mask to stop and release their data.
 * This function will block until the threads exit.
 */
static void drop_replicas(unsigned replicas)
{
    unsigned replica_id;

    ring_buffer_del_reader_mask(&replication_state.rb, replicas);
    while ((replica_id = __builtin_ffs(replicas))) {
        struct replica *r;

        replica_id--;
        assert(replica_id < num_elem(replication_state.replicas));
        r = &replication_state.replicas[replica_id];

        pthread_join(r->thread, NULL);
        release_replica(r);

        replicas ^= 1 << replica_id;
    }
}

void replication_origin_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
    /* TODO We'd really like to avoid at least malloc() here, preferrably also memcpy(). */
    void *p = selva_malloc(buf_size);
    static ring_buffer_eid_t eid;
    unsigned not_replicated;

    eid = (eid + 1) & ~EID_MSB_MASK; /* TODO any better ideas? */
    memcpy(p, buf, buf_size);
    while ((not_replicated = ring_buffer_insert(&replication_state.rb, eid, p, buf_size))) {
        drop_replicas(not_replicated);
    }
}

void replication_origin_stop()
{
    /* TODO Implement STOP */
}

void replication_origin_init(void)
{
#if 0
    int nr_cores = get_nprocs();
#endif

    for (unsigned i = 0; i < MAX_REPLICAS; i++) {
        struct replica *r = &replication_state.replicas[i];

        r->id = i;
    /* TODO Do global core mapping */
#if 0
        r->core_id = i % nr_cores;
#endif
        r->rb = &replication_state.rb;
    }
    ring_buffer_init(&replication_state.rb, replication_state.buffer, num_elem(replication_state.buffer), free_replbuf);
}
