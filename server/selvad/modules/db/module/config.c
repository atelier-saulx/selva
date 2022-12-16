/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "selva_error.h"
#include "selva_log.h"
#include "selva_db.h"
#include "modinfo.h"
#include "config.h"

struct selva_glob_config selva_glob_config = {
    .debug_modify_replication_delay_ns = DEBUG_MODIFY_REPLICATION_DELAY_NS,
    .hierarchy_initial_vector_len = HIERARCHY_INITIAL_VECTOR_LEN,
    .hierarchy_expected_resp_len = HIERARCHY_EXPECTED_RESP_LEN,
    .hierarchy_compression_level = HIERARCHY_COMPRESSION_LEVEL,
    .hierarchy_auto_compress_period_ms = HIERARCHY_AUTO_COMPRESS_PERIOD_MS,
    .hierarchy_auto_compress_old_age_lim = HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM,
    .find_indices_max = FIND_INDICES_MAX,
    .find_indexing_threshold = FIND_INDEXING_THRESHOLD,
    .find_indexing_icb_update_interval = FIND_INDEXING_ICB_UPDATE_INTERVAL,
    .find_indexing_interval = FIND_INDEXING_INTERVAL,
    .find_indexing_popularity_ave_period = FIND_INDEXING_POPULARITY_AVE_PERIOD,
};

static int parse_size_t(void *dst, const char *src)
{
    long long v;
    char *endptr = (char *)src;
    size_t *d = (size_t *)dst;

    v = strtoull(src, &endptr, 10);
    if (endptr == src) {
        return SELVA_EINVAL;
    }

    *d = (size_t)v;

    return 0;
}

static int parse_int(void *dst, const char *src)
{
    long long v;
    char *endptr = (char *)src;
    int *d = (int *)dst;

    v = strtol(src, &endptr, 10);
    if (endptr == src) {
        return SELVA_EINVAL;
    }

    *d = (int)v;

    return 0;
}

struct cfg {
    const char * const name;
    int (*const parse)(void *dst, const char *src);
    void * const dp;
} const cfg_map[] = {
    { "DEBUG_MODIFY_REPLICATION_DELAY_NS", parse_int, &selva_glob_config.debug_modify_replication_delay_ns },
    { "HIERARCHY_INITIAL_VECTOR_LEN", parse_size_t, &selva_glob_config.hierarchy_initial_vector_len },
    { "HIERARCHY_EXPECTED_RESP_LEN",  parse_size_t, &selva_glob_config.hierarchy_expected_resp_len },
    { "HIERARCHY_COMPRESSION_LEVEL", parse_int, &selva_glob_config.hierarchy_compression_level },
    { "HIERARCHY_AUTO_COMPRESS_PERIOD_MS", parse_int, &selva_glob_config.hierarchy_auto_compress_period_ms },
    { "HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM", parse_int, &selva_glob_config.hierarchy_auto_compress_old_age_lim },
    { "FIND_INDICES_MAX", parse_int, &selva_glob_config.find_indices_max },
    { "FIND_INDEXING_THRESHOLD", parse_int, &selva_glob_config.find_indexing_threshold },
    { "FIND_INDEXING_ICB_UPDATE_INTERVAL", parse_int, &selva_glob_config.find_indexing_icb_update_interval },
    { "FIND_INDEXING_INTERVAL", parse_int, &selva_glob_config.find_indexing_interval },
    { "FIND_INDEXING_POPULARITY_AVE_PERIOD", parse_int, &selva_glob_config.find_indexing_popularity_ave_period },
};

int parse_config_args(void)
{
    for (size_t i = 0; i < num_elem(cfg_map); i++) {
        struct cfg const * const cfg = &cfg_map[i];
        const char *name = cfg->name;
        const char *str;
        int err;

        str = getenv(name);
        if (!str) {
            continue;
        }

        err = cfg->parse(cfg->dp, str);
        if (err) {
            return err;
        }

        SELVA_LOG_DBG("Selva tunable changed: %s\n", cfg->name);
    }

    return 0;
}

/* FIXME Implement SELVA_MODINFO */
#if 0
static void mod_info(RedisModuleInfoCtx *ctx)
{
    for (size_t i = 0; i < num_elem(cfg_map); i++) {
        struct cfg const * const cfg = &cfg_map[i];
        char *name = (char *)cfg->name;

        if (cfg->parse == &parse_size_t) {
            (void)RedisModule_InfoAddFieldULongLong(ctx, name, *(size_t *)cfg->dp);
        } else if (cfg->parse == &parse_int) {
            (void)RedisModule_InfoAddFieldLongLong(ctx, name, *(int *)cfg->dp);
        } else {
            (void)RedisModule_InfoAddFieldCString(ctx, name, "Unsupported type");
        }
    }
}
SELVA_MODINFO("config", mod_info);
#endif
