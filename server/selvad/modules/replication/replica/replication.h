/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Get the EID of the newest SDB dump point.
 */
uint64_t replication_replica_get_last_sdb_eid(void);

uint64_t replication_replica_get_last_cmd_eid(void);

void replication_replica_new_sdb(const struct selva_string *filename, const uint8_t sdb_hash[SELVA_IO_HASH_SIZE]);

/**
 * Start replicating from an origin server.
 */
int replication_replica_start(struct sockaddr_in *origin_addr);
void replication_replica_init(void);
