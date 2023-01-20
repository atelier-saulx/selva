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
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_server.h"
#include "ring_buffer.h"
#include "replication.h"

/* TODO This should be same as SDB HASH_SIZE */
#define HASH_SIZE 32
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

void replication_new_sdb(char sdb_hash[HASH_SIZE])
{
    memcpy(replication_state.sdb_hash, sdb_hash, HASH_SIZE);
    replication_state.sdb_eid = gen_sdb_eid(sdb_hash);
    ring_buffer_insert(&replication_state.rb, replication_state.sdb_eid, replication_state.sdb_hash);
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
static struct replica *new_replica(void)
{
    for (int i = 0; i < MAX_REPLICAS; i++) {
        if (!replication_state.replicas[i].in_use) {
            struct replica *r = &replication_state.replicas[i];

            r->in_use = 1;
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

static void start_replica_thread(struct sockaddr_in *serv_addr)
{
    struct replica *replica = new_replica();

    if (!replica) {
        return; /* TODO ERROR */
    }

    ring_buffer_add_reader(&replication_state.rb, replica->id);
    pthread_create(&replica->thread, NULL, replication_thread, replica);
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

void replication_replicate(int8_t cmd, const void *buf, size_t buf_size)
{
    /* TODO We'd really like to avoid at least malloc() here, preferrably also memcpy(). */
    void *p = selva_malloc(sizeof(buf_size) + buf_size);
    static ring_buffer_eid_t eid;
    unsigned not_replicated;

    eid = (eid + 1) & ~EID_MSB_MASK; /* TODO any better ideas? */
    memcpy(p, &buf_size, sizeof(buf_size));
    memcpy(p + sizeof(buf_size), buf, buf_size);
    while ((not_replicated = ring_buffer_insert(&replication_state.rb, eid, p))) {
        drop_replicas(not_replicated);
    }
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

    int nr_cores = get_nprocs();

    for (unsigned i = 0; i < MAX_REPLICAS; i++) {
        struct replica *r = &replication_state.replicas[i];

        r->id = i;
        r->core_id = i % nr_cores;
        r->rb = &replication_state.rb;
    }
    ring_buffer_init(&replication_state.rb, replication_state.buffer, num_elem(replication_state.buffer), free_replbuf);

    SELVA_MK_COMMAND(CMD_REPLICAOF_ID, replicaof);
    SELVA_MK_COMMAND(CMD_REPLICAINFO_ID, replicainfo);
}
