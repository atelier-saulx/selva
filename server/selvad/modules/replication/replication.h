/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_server_response_out;
struct selva_string;
struct sockaddr_in;

/**
 * Replication sync command mode.
 */
enum replication_sync_mode {
    REPLICATION_SYNC_MODE_FULL = 0, /*!< Start by sending a full dump. */
    REPLICATION_SYNC_MODE_PARTIAL, /*!< Start based on a previously sent dump. */
};

/**
 * Last SDB loaded.
 * Updated by selva_replication_new_sdb().
 */
extern uint8_t last_sdb_hash[SELVA_IO_HASH_SIZE];

/**
 * Origin must implement.
 * @{
 */

void replication_origin_new_sdb(const char *filename, uint8_t sdb_hash[SELVA_IO_HASH_SIZE]);

/**
 * Insert a new sdb structure that is marked as incomplete.
 * The intention is that we can keep replicating commands for existing replicas
 * while a new SDB dump is being created asynchronously.
 * @returns sdb_eid.
 */
uint64_t replication_origin_new_incomplete_sdb(const char *filename);

/**
 * Finalize an incomplete sdb structure.
 */
void replication_origin_complete_sdb(uint64_t sdb_eid, uint8_t sdb_hash[SELVA_IO_HASH_SIZE]);

/**
 * Get the EID of the newest SDB dump point.
 */
uint64_t replication_origin_get_last_sdb_eid(void);

/**
 * Get the EID if the last command received for replication.
 */
uint64_t replication_origin_get_last_cmd_eid(void);

uint64_t replication_origin_get_last_eid(void);

/**
 * Replicate a command to replicas.
 */
void replication_origin_replicate(int64_t ts, int8_t cmd, const void *buf, size_t buf_size);

/**
 * Replicate a command to replicas.
 * Pass the ownership of buf to the replication module. Avoids one malloc.
 * buf must be allocated with `selva_malloc` or `selva_realloc`.
 */
void replication_origin_replicate_pass(int64_t ts, int8_t cmd, void *buf, size_t buf_size);

/**
 * Check that the eid exists in the rb and the file still exists.
 */
int replication_origin_check_sdb(uint64_t eid);

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

unsigned replication_origin_get_replicas_mask(void);

/**
 * Find replica by resp connection.
 * The resp structure doesn't need to be the one registered with
 * replication_origin_register_replica() but any selva_server_response_out that
 * utilizes the same connection.
 */
int replication_origin_find_replica(struct selva_server_response_out *resp, unsigned *replication_id);

struct selva_server_response_out *replication_origin_get_replica_resp(unsigned replica_id);

/**
 * Update current eid as reported by the replica.
 */
void replication_origin_update_replica_last_ack(unsigned replica_id, uint64_t eid);

/**
 * Returns the last ack'd eid.
 */
uint64_t replication_origin_get_replica_last_ack(unsigned replica_id);

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

/**
 * Get the EID if the last replicated command.
 */
uint64_t replication_replica_get_last_cmd_eid(void);

/**
 * Register new sdb dump.
 * @returns sdb_eid.
 */
uint64_t replication_replica_new_sdb(const char *filename);

/**
 * Returns 1 if the replica believes it's in a stale state.
 */
int replication_replica_is_stale(void);

#ifdef CONN_STR_LEN
size_t replication_replica_origin2str(char buf[CONN_STR_LEN]);
#endif

/**
 * Start replicating from an origin server.
 */
int replication_replica_start(struct sockaddr_in *origin_addr);
void replication_replica_init(void);

/**
 * @}
 */
