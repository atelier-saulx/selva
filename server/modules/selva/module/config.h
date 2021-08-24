#pragma once
#ifndef _CONFIG_H_
#define _CONFIG_H_

struct RedisModuleString;

struct selva_glob_config {
    size_t hierarchy_initial_vector_len;
    size_t hierarchy_expected_resp_len;
    int find_lfu_count_init;
    int find_lfu_count_incr;
    int find_lfu_count_create;
    int find_lfu_count_discard;
};

extern struct selva_glob_config selva_glob_config;

int parse_config_args(struct RedisModuleString **argv, int argc);

#endif /* _CONFIG_H_ */
