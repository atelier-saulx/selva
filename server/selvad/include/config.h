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

EVL_EXPORT(int, config_resolve, const struct config cfg_map[], size_t len);
