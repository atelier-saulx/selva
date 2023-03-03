/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

enum replication_sync_mode {
    REPLICATION_SYNC_MODE_FULL = 0,
    REPLICATION_SYNC_MODE_PARTIAL,
};

struct selva_server_response_out;
struct selva_string;
struct sockaddr_in;

/**
 * Last SDB loaded.
 * Updated by selva_replication_new_sdb().
 */
extern uint8_t last_sdb_hash[SELVA_IO_HASH_SIZE];

/**
 * Origin must implement.
 * @{
 */

void replication_origin_new_sdb(const struct selva_string *filename);

/**
 * Get the EID of the newest SDB dump point.
 */
uint64_t replication_origin_get_last_sdb_eid(void);

uint64_t replication_origin_get_last_cmd_eid(void);

/**
 * Replicate a command to replicas.
 */
void replication_origin_replicate(int8_t cmd, const void *buf, size_t buf_size);

/**
 * Register a new replication client aka replica.
 * @param resp is a selva_server_response_out that allows sending data to the replica.
 * @returns 0 if successful; Otherwise a selva error is returned.
 */
int replication_origin_register_replica(
        struct selva_server_response_out *resp,
        uint64_t start_eid,
        const uint8_t start_sdb_hash[SELVA_IO_HASH_SIZE],
        enum replication_sync_mode mode);

/**
 * Initialize this node as an origin.
 * This is an irreversible operation.
 */
void replication_origin_init(void);

/**
 * @}
 */

/**
 * Replica must implement.
 * @{
 */

/**
 * Get the EID of the newest SDB dump point.
 */
uint64_t replication_replica_get_last_sdb_eid(void);

uint64_t replication_replica_get_last_cmd_eid(void);

void replication_replica_new_sdb(const struct selva_string *filename);

/**
 * Start replicating from an origin server.
 */
int replication_replica_start(struct sockaddr_in *origin_addr);
void replication_replica_init(void);

/**
 * @}
 */
