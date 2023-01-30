/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct replica {
    int in_use; /*!< Used for alloc. See new_replica() and release_replica(). */
    unsigned id; /*!< Replica id. */
    struct selva_thread thread;
    ring_buffer_eid_t start_eid;
    struct ring_buffer *rb; /*!< Pointer to the shared ring_buffer. */
    struct selva_server_response_out *resp; /*!< Response stream to the replica (client). */
};

/* replica_thread.c */
void *replication_thread(void *arg);

