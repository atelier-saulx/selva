#pragma once
#ifndef _CONFIG_H_
#define _CONFIG_H_

struct RedisModuleString;

/**
 * A structure type of global config params that can be changed at startup.
 * See tunables.h for the description and default values of these parameters.
 */
struct selva_glob_config {
    size_t hierarchy_initial_vector_len;
    size_t hierarchy_expected_resp_len;
    int find_indices_max;
    int find_indexing_threshold;
    int find_indexing_icb_update_interval;
    int find_indexing_interval;
    int find_indexing_popularity_ave_period;
};

extern struct selva_glob_config selva_glob_config;

int parse_config_args(struct RedisModuleString **argv, int argc);

#endif /* _CONFIG_H_ */
