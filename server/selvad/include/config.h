/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "_evl_export.h"

enum config_type {
    CONFIG_CSTRING = 0,
    CONFIG_INT,
    CONFIG_SIZE_T,
};

struct config {
    const char * const name;
    enum config_type type;
    void * const dp;
};

struct config_list {
    const char *mod_name;
    const struct config *cfg_map;
    size_t len;
};

/**
 * Resolve and register config variables.
 * All pointers must be statically allocated.
 * @param mod_name is the name of the module resolving this config.
 */
EVL_EXPORT(int, config_resolve, const char *mod_name, const struct config cfg_map[], size_t len);
EVL_EXPORT(size_t, config_list_get, const struct config_list **out);
