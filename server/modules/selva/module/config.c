#include <string.h>
#include <stddef.h>
#include <redismodule.h>
#include "cdefs.h"
#include "errors.h"
#include "config.h"

struct selva_glob_config selva_glob_config = {
    .hierarchy_initial_vector_len = HIERARCHY_INITIAL_VECTOR_LEN,
    .hierarchy_expected_resp_len = HIERARCHY_EXPECTED_RESP_LEN,
    .find_indices_max = FIND_INDICES_MAX,
    .find_indexing_threshold = FIND_INDEXING_THRESHOLD,
    .find_indexing_icb_update_interval = FIND_INDEXING_ICB_UPDATE_INTERVAL,
    .find_indexing_interval = FIND_INDEXING_INTERVAL,
    .find_indexing_popularity_ave_period = FIND_INDEXING_POPULARITY_AVE_PERIOD,
};

static int parse_size_t(void *dst, const RedisModuleString *src) {
    long long v;
    size_t *d = (size_t *)dst;

    if (RedisModule_StringToLongLong(src, &v) == REDISMODULE_ERR) {
        return SELVA_EINVAL;
    }

    *d = (size_t)v;

    return 0;
}

static int parse_int(void *dst, const RedisModuleString *src) {
    long long v;
    int *d = (int *)dst;

    if (RedisModule_StringToLongLong(src, &v) == REDISMODULE_ERR) {
        return SELVA_EINVAL;
    }

    *d = (size_t)v;

    return 0;
}

struct cfg {
    const char * const name;
    int (*const parse)(void *dst, const RedisModuleString *sp);
    void * const dp;
} const cfg_map[] = {
    { "HIERARCHY_INITIAL_VECTOR_LEN", parse_size_t, &selva_glob_config.hierarchy_initial_vector_len },
    { "HIERARCHY_EXPECTED_RESP_LEN",  parse_size_t, &selva_glob_config.hierarchy_expected_resp_len },
    { "FIND_INDICES_MAX", parse_int, &selva_glob_config.find_indices_max },
    { "FIND_INDEXING_THRESHOLD", parse_int, &selva_glob_config.find_indexing_threshold },
    { "FIND_INDEXING_ICB_UPDATE_INTERVAL", parse_int, &selva_glob_config.find_indexing_icb_update_interval },
    { "FIND_INDEXING_INTERVAL", parse_int, &selva_glob_config.find_indexing_interval },
    { "FIND_INDEXING_POPULARITY_AVE_PERIOD", parse_int, &selva_glob_config.find_indexing_popularity_ave_period },
};

int parse_config_args(RedisModuleString **argv, int argc) {
    while(argc--) {
        const char * const cfg_name = RedisModule_StringPtrLen(*argv, NULL);
        int found = 0;

        for (size_t i = 0; i < num_elem(cfg_map); i++) {
            struct cfg const * const cfg = &cfg_map[i];
            if (!strcmp(cfg_name, cfg->name)) {
                int err;

                err = cfg->parse(cfg->dp, argv[1]);
                if (err) {
                    return err;
                }

#if 0
                fprintf(stderr, "Selva tunable changed: %s\n", cfg->name);
#endif

                found = 1;
                break;
            }
        }

        if (!found) {
            return SELVA_EINVAL;
        }
    }

    return 0;
}
