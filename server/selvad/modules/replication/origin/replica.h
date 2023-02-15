/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct replica {
    int in_use; /*!< Used for alloc. See new_replica() and release_replica(). */
    unsigned id; /*!< Replica id. */
    struct selva_thread thread;
    ring_buffer_eid_t start_eid; /*!< Must point to an sdb dump. */
    struct ring_buffer *rb; /*!< Pointer to the shared ring_buffer. */
    struct selva_server_response_out *resp; /*!< Response stream to the replica (client). */
};

/**
 * SDB info stored in the rb.
 * The EID of this structure must be OR'ed with EID_MSB_MASK.
 */
struct sdb {
    /**
     * Filename of the dump.
     * This file most not be overwritten.
     */
    struct selva_string *filename;

    /**
     * The hash of the dump.
     */
    uint8_t hash[HASH_SIZE];
};

/* replica_thread.c */
void *replication_thread(void *arg);

