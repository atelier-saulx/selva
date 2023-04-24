/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct replica {
    /* -- Origin writes -- */
    int in_use; /*!< Used for alloc. See new_replica() and release_replica(). */
    unsigned id; /*!< Replica id. */
    struct selva_thread thread;
    enum replication_sync_mode sync_mode;
    ring_buffer_eid_t start_eid; /*!< Must point to an sdb dump. */
    uint64_t ack_eid; /*!< Last ack'd eid by the replica. */
    uint8_t start_sdb_hash[SELVA_IO_HASH_SIZE];
    /* -- Replica writes -- */
    ring_buffer_eid_t current_eid;
    struct ring_buffer *rb; /*!< Pointer to the shared ring_buffer. */
    struct selva_server_response_out *resp; /*!< Response stream to the replica (client). */
};

/**
 * SDB info stored in the origin rb.
 * The EID of this structure must be OR'ed with EID_MSB_MASK.
 */
struct selva_replication_sdb {
    enum {
        SDB_STATUS_INVALID = 0x00,
        SDB_STATUS_INCOMPLETE = 0x01,
        SDB_STATUS_COMPLETE = 0x02,
    } status;

    /**
     * The hash of the dump.
     */
    uint8_t hash[SELVA_IO_HASH_SIZE];

    /**
     * Filename of the dump.
     * This file must not be overwritten.
     */
    char filename[];
};

/* replica_thread.c */
void *replication_thread(void *arg);

