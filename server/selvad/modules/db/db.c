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
#include "evl_signal.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "dump.h"
#include "selva_onload.h"
#include "selva_server.h"
#include "selva_io.h"
#include "selva_replication.h"
#include "config.h"
#include "db_config.h"
#include "selva_db.h"

struct selva_glob_config selva_glob_config = {
    .debug_modify_replication_delay_ns = 0,
    .hierarchy_initial_vector_len = 0,
    .hierarchy_expected_resp_len = 5000,
    .hierarchy_compression_level = 6,
    .hierarchy_auto_compress_period_ms = 0,
    .hierarchy_auto_compress_old_age_lim = 100,
    .find_indices_max = 0,
    .find_indexing_threshold = 100,
    .find_indexing_icb_update_interval = 5000,
    .find_indexing_interval = 60000,
    .find_indexing_popularity_ave_period = 216000,
    .auto_save_interval = 0,
};

static const struct config cfg_map[] = {
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
    { "AUTO_SAVE_INTERVAL",                     CONFIG_INT,     &selva_glob_config.auto_save_interval },
};

SET_DECLARE(selva_onload, Selva_Onload);
SET_DECLARE(selva_onunld, Selva_Onunload);

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(evl_set_timeout);
    evl_import_main(evl_clear_timeout);
    evl_import_main(config_resolve);
    evl_import_event_loop();
	evl_import_signal();
    import_selva_server();
    import_selva_io();
    import_selva_replication();
}

__constructor void init(void)
{
    int err;
    Selva_Onload **onload_p;

    evl_module_init("db");

    libdeflate_set_memory_allocator(selva_malloc, selva_free);

    err = config_resolve("db", cfg_map, num_elem(cfg_map));
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

    err = dump_load_default_sdb();
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to load the default dump: %s",
                  selva_strerror(err));
        exit(EXIT_FAILURE);
    }

    if (selva_glob_config.auto_save_interval > 0 &&
        dump_auto_sdb(selva_glob_config.auto_save_interval)) {
        exit(EXIT_FAILURE);
    }
}

__destructor void deinit(void) {
    Selva_Onunload **onunload_p;

    SET_FOREACH(onunload_p, selva_onunld) {
        Selva_Onunload *onunload = *onunload_p;

        onunload();
    }
}
