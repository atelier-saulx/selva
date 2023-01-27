/*
 * Selva Replication Module.
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

#if SELVA_REPLICATION_MAIN
#define SELVA_REPLICATION_EXPORT(_ret_, _fun_name_, ...) _ret_ _fun_name_(__VA_ARGS__) EVL_EXTERN
#else
#define SELVA_REPLICATION_EXPORT(_ret_, _fun_name_, ...) _ret_ (*_fun_name_)(__VA_ARGS__) EVL_COMMON
#endif

/* TODO This should be same as SDB HASH_SIZE and shouldn't be copy-pasted here */
#define HASH_SIZE 32

SELVA_REPLICATION_EXPORT(void, selva_replication_new_sdb, char sdb_hash[HASH_SIZE]);
SELVA_REPLICATION_EXPORT(void, selva_replication_replicate, int8_t cmd, const void *buf, size_t buf_size);
SELVA_REPLICATION_EXPORT(void, selva_replication_stop, void);

#define _import_selva_replication(apply) \
    apply(selva_replication_new_sdb) \
    apply(selva_replication_replicate) \
    apply(selva_replication_stop)

#define _import_selva_replication1(f) \
    evl_import(f, "mod_replication.so");

/**
 * Import all symbols from selva_io.h.
 */
#define import_selva_replication() \
    _import_selva_replication(_import_selva_replication1)
