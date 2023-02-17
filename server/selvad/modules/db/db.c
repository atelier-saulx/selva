/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "jemalloc.h"
#include "libdeflate.h"
#include "linker_set.h"
#include "event_loop.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_onload.h"
#include "selva_server.h"
#include "selva_io.h"
#include "selva_replication.h"
#include "config.h"
#include "db_config.h"
#include "selva_db.h"

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
    .redis_addr = "127.0.0.1",
    .redis_port = 6379,
};

const struct config cfg_map[] = {
    { "DEBUG_MODIFY_REPLICATION_DELAY_NS",      CONFIG_INT,     &selva_glob_config.debug_modify_replication_delay_ns },
    { "HIERARCHY_INITIAL_VECTOR_LEN",           CONFIG_SIZE_T,  &selva_glob_config.hierarchy_initial_vector_len },
    { "HIERARCHY_EXPECTED_RESP_LEN",            CONFIG_SIZE_T,  &selva_glob_config.hierarchy_expected_resp_len },
    { "HIERARCHY_COMPRESSION_LEVEL",            CONFIG_INT,     &selva_glob_config.hierarchy_compression_level },
    { "HIERARCHY_AUTO_COMPRESS_PERIOD_MS",      CONFIG_INT,     &selva_glob_config.hierarchy_auto_compress_period_ms },
    { "HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM",    CONFIG_INT,     &selva_glob_config.hierarchy_auto_compress_old_age_lim },
    { "FIND_INDICES_MAX",                       CONFIG_INT,     &selva_glob_config.find_indices_max },
    { "FIND_INDEXING_THRESHOLD",                CONFIG_INT,     &selva_glob_config.find_indexing_threshold },
    { "FIND_INDEXING_ICB_UPDATE_INTERVAL",      CONFIG_INT,     &selva_glob_config.find_indexing_icb_update_interval },
    { "FIND_INDEXING_INTERVAL",                 CONFIG_INT,     &selva_glob_config.find_indexing_interval },
    { "FIND_INDEXING_POPULARITY_AVE_PERIOD",    CONFIG_INT,     &selva_glob_config.find_indexing_popularity_ave_period },
    { "REDIS_ADDR",                             CONFIG_CSTRING, &selva_glob_config.redis_addr },
    { "REDIS_PORT",                             CONFIG_INT,     &selva_glob_config.redis_port },
};

SET_DECLARE(selva_onload, Selva_Onload);
SET_DECLARE(selva_onunld, Selva_Onunload);

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(evl_set_timeout);
    evl_import_main(evl_clear_timeout);
    evl_import_main(config_resolve);
    evl_import_event_loop();
    import_selva_server();
    import_selva_io();
    import_selva_replication();
}

__constructor void init(void)
{
    int err;
    Selva_Onload **onload_p;

    SELVA_LOG(SELVA_LOGL_INFO, "Init db");

    libdeflate_set_memory_allocator(selva_malloc, selva_free);

    err = config_resolve(cfg_map, num_elem(cfg_map));
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to parse config args: %s",
                  selva_strerror(err));
        exit(EXIT_FAILURE);
    }

    SET_FOREACH(onload_p, selva_onload) {
        Selva_Onload *onload = *onload_p;

        err = onload();
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to init db: %s",
                      selva_strerror(err));
            exit(EXIT_FAILURE);
        }
    }
}

__destructor void deinit(void) {
    Selva_Onunload **onunload_p;

    SET_FOREACH(onunload_p, selva_onunld) {
        Selva_Onunload *onunload = *onunload_p;

        onunload();
    }
}
