/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/* TODO This should be same as SDB HASH_SIZE */
#define HASH_SIZE 32

void replication_origin_new_sdb(char sdb_hash[HASH_SIZE]);
void replication_origin_replicate(int8_t cmd, const void *buf, size_t buf_size);
int replication_origin_register_replica(void /* TODO client? */);
void replication_origin_stop();
void replication_origin_init(void);
