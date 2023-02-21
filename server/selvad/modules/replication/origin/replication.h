/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void replication_origin_new_sdb(const struct selva_string *filename, const uint8_t sdb_hash[SELVA_IO_HASH_SIZE]);

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
int replication_origin_register_replica(struct selva_server_response_out *resp, uint64_t start_eid);

/**
 * Initialize this node as an origin.
 * This is an irreversible operation.
 */
void replication_origin_init(void);
