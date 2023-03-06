/*
 * Selva Replication Module.
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"
#include "_selva_io.h"

#if SELVA_REPLICATION_MAIN
#define SELVA_REPLICATION_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) EVL_EXTERN
#else
#define SELVA_REPLICATION_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) EVL_COMMON
#endif

/**
 * Marks an EID used for SDB.
 * Used to distinguish between an SDB and command, which is useful
 * with data structures that can contain both using the same pointers.
 */
#define EID_MSB_MASK (~(~(typeof(uint64_t))0 >> 1))

/**
 * Publish a new SDB dump to the replication.
 */
SELVA_REPLICATION_EXPORT(void, selva_replication_new_sdb, const struct selva_string *filename, const uint8_t sdb_hash[SELVA_IO_HASH_SIZE]);

/**
 * Replicate a command buffer to the replicas.
 * This is a NOP for replication modes other than ORIGIN.
 */
SELVA_REPLICATION_EXPORT(void, selva_replication_replicate, int8_t cmd, const void *buf, size_t buf_size);

#define _import_selva_replication(apply) \
    apply(selva_replication_new_sdb) \
    apply(selva_replication_replicate)

#define _import_selva_replication1(f) \
    evl_import(f, "mod_replication.so");

/**
 * Import all symbols from selva_io.h.
 */
#define import_selva_replication() \
    _import_selva_replication(_import_selva_replication1)
