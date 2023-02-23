/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_CONFIG_H_
#define _SELVA_CONFIG_H_

/**
 * A structure type of global config params that can be changed at startup.
 * See db.c for the default values of these parameters.
 */
struct selva_glob_config {
    /**
     * Add delay to the replication of the Modify command.
     * Unit is nanoseconds. Normally this should be set to 0.
     */
    int debug_modify_replication_delay_ns;
    /**
     * Initial vector lengths for children and parents lists.
     */
    size_t hierarchy_initial_vector_len;
    /**
     * Expected average length of a find response.
     */
    size_t hierarchy_expected_resp_len;
    /**
     * Compression level used for compressing subtrees.
     * Range: 1 - 12
     */
    int hierarchy_compression_level;
    /**
     * Attempt to compress inactive nodes in-memory.
     * 0 Disables automatic compression.
     */
    int hierarchy_auto_compress_period_ms;
    /**
     * Hierarchy auto compression transaction age limit.
     */
    int hierarchy_auto_compress_old_age_lim;
    /**
     * Maximum number of indices.
     * 0 = disable indexing.
     */
    int find_indices_max;
    /**
     * A candidate for indexing must have at least this many visits per traversal.
     */
    int find_indexing_threshold;
    /**
     * [ms] ICB refresh interval.
     */
    int find_indexing_icb_update_interval;
    /**
     * How often the set of active indices is decided.
     */
    int find_indexing_interval;
    /**
     * [sec] Averaging period for indexing hint demand count. After this period the original value is reduced to 1/e * n.
     */
    int find_indexing_popularity_ave_period;

    /**
     * Redis used for publishing subscription changes.
     */
    const char *redis_addr;
    int redis_port;

    /**
     * [sec] Load the default SDB on startup and save a dump on interval.
     * 0 = disabled.
     */
    int auto_save_interval;
};

extern struct selva_glob_config selva_glob_config;

#endif /* _SELVA_CONFIG_H_ */
