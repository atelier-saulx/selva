/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_CONFIG_H_
#define _SELVA_CONFIG_H_

/**
 * A structure type of global config params that can be changed at startup.
 * See tunables.h for the description and default values of these parameters.
 */
struct selva_glob_config {
    int debug_modify_replication_delay_ns;
    size_t hierarchy_initial_vector_len;
    size_t hierarchy_expected_resp_len;
    int hierarchy_compression_level;
    int hierarchy_auto_compress_period_ms;
    int hierarchy_auto_compress_old_age_lim;
    int find_indices_max;
    int find_indexing_threshold;
    int find_indexing_icb_update_interval;
    int find_indexing_interval;
    int find_indexing_popularity_ave_period;
    const char *redis_addr;
    int redis_port;
};

extern struct selva_glob_config selva_glob_config;

#endif /* _SELVA_CONFIG_H_ */
