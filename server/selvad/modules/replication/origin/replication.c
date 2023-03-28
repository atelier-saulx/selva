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
#include "ring_buffer.h"
#include "../replication.h"
#include "replica.h"

static_assert(sizeof(ring_buffer_eid_t) >= sizeof(uint64_t));

struct origin_state {
    /*!<
     * Id of the lastest sdb still in rb.
     * If this is zero then a new dump must be made before starting a
     * new replica.
     */
    ring_buffer_eid_t last_pending_sdb_eid;
    ring_buffer_eid_t last_sdb_eid;
    ring_buffer_eid_t last_cmd_eid; /*!< Last cmd eid. */

    /**
     * Ring buffer where the commands to be replicated to the replicas are kept.
     * It's a single ring buffer with a single writer and multiple
     * (multi-threaded) readers, each having their own read index.
     * The writer can only advance if no reader is busy on the next element.
     * However, the writer can force a reader to be dropped i.e. request it
     * to stop blocking/exit.
     * The ring buffer holds both commands and SDB dump references. The
     * dump references are used as sync points to the replicas and if
     * necessary the origin can send the latest full dump to a replica.
     */
    struct ring_buffer rb;
    struct ring_buffer_element buffer[REPLICATION_RING_BUFFER_SIZE];

    struct replica replicas[REPLICATION_MAX_REPLICAS];
};

static struct origin_state origin_state __lazy_alloc_glob;

/**
 * Request replica reader threaders in the mask to stop and release their data.
 * This function will block until the threads exit.
 */
static void drop_replicas(unsigned replicas);

static void insert(ring_buffer_eid_t eid, int64_t ts, int8_t cmd, void *p, size_t p_size) {
    unsigned not_replicated;

    while ((not_replicated = ring_buffer_insert(&origin_state.rb, eid, ts, cmd, p, p_size))) {
        drop_replicas(not_replicated);
    }
}

void replication_origin_new_sdb(const char *filename, uint8_t sdb_hash[SELVA_IO_HASH_SIZE])
{
    size_t filename_len = strlen(filename);
    struct selva_replication_sdb *sdb = selva_malloc(sizeof(struct selva_replication_sdb) + filename_len + 1);
    uint64_t sdb_eid = replication_new_origin_sdb_eid(filename);

    sdb->status = SDB_STATUS_COMPLETE;
    strcpy(sdb->filename, filename);
    memcpy(sdb->hash, sdb_hash, SELVA_IO_HASH_SIZE);

    SELVA_LOG(SELVA_LOGL_INFO, "New SDB: %s (0x%" PRIx64 ")", filename, sdb_eid);

    insert(sdb_eid, (sdb_eid & ~EID_MSB_MASK), 0, sdb, sizeof(*sdb));
    origin_state.last_sdb_eid = sdb_eid;
    origin_state.last_cmd_eid = 0;
}

uint64_t replication_origin_new_incomplete_sdb(const char *filename)
{
    size_t filename_len = strlen(filename);
    struct selva_replication_sdb *sdb = selva_malloc(sizeof(struct selva_replication_sdb) + filename_len + 1);
    uint64_t sdb_eid = replication_new_origin_sdb_eid(filename);

    sdb->status = SDB_STATUS_INCOMPLETE;
    strcpy(sdb->filename, filename);
    insert(sdb_eid, (sdb_eid & ~EID_MSB_MASK), 0, sdb, sizeof(*sdb));

    /*
     * This is set so that we can always have cmd_eids be greater than sdb dumps
     * created previously in time. It's not an issue if the dump fails and this
     * ends up never being used because eids grow monotonically anyway.
     */
    origin_state.last_pending_sdb_eid = sdb_eid;

    return sdb_eid;
}

void replication_origin_complete_sdb(uint64_t sdb_eid, uint8_t sdb_hash[SELVA_IO_HASH_SIZE])
{
    struct ring_buffer_element *rb_elem;
    struct selva_replication_sdb *sdb;

    assert(sdb_eid & EID_MSB_MASK);

    rb_elem = ring_buffer_writer_find(&origin_state.rb, sdb_eid);
    if (!rb_elem) {
        SELVA_LOG(SELVA_LOGL_WARN,
                  "Ring buffer elem for an SDB (0x%" PRIx64 ") is already gone",
                  sdb_eid);

        return;
    }

    sdb = (struct selva_replication_sdb *)rb_elem->data;
    assert(rb_elem->data_size == sizeof(struct selva_replication_sdb));
    assert(sdb && sdb->status == SDB_STATUS_INCOMPLETE);

    sdb->status = SDB_STATUS_COMPLETE;
    memcpy(sdb->hash, sdb_hash, SELVA_IO_HASH_SIZE);

    SELVA_LOG(SELVA_LOGL_INFO, "New SDB: %s (0x%" PRIx64 ")", sdb->filename, sdb_eid);

    origin_state.last_sdb_eid = sdb_eid;

    /*
     * Not a perfect solution but this way we'll usually show rhe right info for
     * `replicainfo` and it's unlikely that we'd zero it if there were new
     * commands after this sdb_eid was issued.
     */
    if (origin_state.last_cmd_eid < (sdb_eid & ~EID_MSB_MASK)) {
        origin_state.last_cmd_eid = 0;
    }
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
        struct selva_replication_sdb *sdb = (struct selva_replication_sdb *)buf;

        if (eid == origin_state.last_sdb_eid) {
            origin_state.last_sdb_eid = 0;
        }

        selva_free(sdb);
    } else {
        selva_free(buf);
    }
}

/**
 * Allocate a new replica_id.
 */
static struct replica *new_replica(struct selva_server_response_out *resp)
{
    for (int i = 0; i < REPLICATION_MAX_REPLICAS; i++) {
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
        int err;

        replica_id--;
        assert(replica_id < num_elem(origin_state.replicas));
        r = &origin_state.replicas[replica_id];

        err = pthread_join(r->thread.pthread, NULL);
        if (err) {
#if __GLIBC__
            SELVA_LOG(SELVA_LOGL_ERR, "pthread_join() failed: %s",
                      strerrorname_np(err) ?: "Unknown error");
#else
            /*
             * There is a library called errnoname that would print the proper
             * name but it's probably overkill to use it for just one line in
             * the whole project.
             */
            SELVA_LOG(SELVA_LOGL_ERR, "pthread_join() failed: %d", err);
#endif
        }
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
    const ring_buffer_eid_t pending_sdb_eid_um = origin_state.last_pending_sdb_eid & ~EID_MSB_MASK;
    ring_buffer_eid_t eid;

    if (pending_sdb_eid_um > sdb_eid_um && pending_sdb_eid_um > origin_state.last_cmd_eid) {
        origin_state.last_cmd_eid = origin_state.last_pending_sdb_eid;
    } else if (sdb_eid_um > origin_state.last_cmd_eid) {
        origin_state.last_cmd_eid = sdb_eid_um;
    }
    eid = origin_state.last_cmd_eid = (origin_state.last_cmd_eid + 1) & ~EID_MSB_MASK;

    return eid;
}

void replication_origin_replicate(int64_t ts, int8_t cmd, const void *buf, size_t buf_size)
{
    void *p = selva_malloc(buf_size);

    memcpy(p, buf, buf_size);
    insert(next_eid(), ts, cmd, p, buf_size);
}

void replication_origin_replicate_pass(int64_t ts, int8_t cmd, void *buf, size_t buf_size)
{
    insert(next_eid(), ts, cmd, buf, buf_size);
}

void replication_origin_init(void)
{
    memset(&origin_state, 0, sizeof(origin_state));

    for (unsigned i = 0; i < REPLICATION_MAX_REPLICAS; i++) {
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
