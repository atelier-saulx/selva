/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct replica {
    int in_use;
    unsigned id;
    int core_id;
    pthread_t thread;
    struct ring_buffer *rb; /*!< Pointer to the shared ring_buffer. */
};

/* replica_thread.c */
void *replication_thread(void *arg);
