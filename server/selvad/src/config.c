/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdlib.h>
#include "selva_log.h"
#include "selva_error.h"
#include "config.h"

static struct config_list config_list[100];
static size_t config_list_next;

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

static void config_list_insert(const char *mod_name, const struct config cfg_map[], size_t len)
{
    if (config_list_next == num_elem(config_list)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Config list full");
        return;
    }

    config_list[config_list_next++] = (struct config_list){
        .mod_name = mod_name,
        .cfg_map = cfg_map,
        .len = len,
    };
}

size_t config_list_get(const struct config_list **out)
{
    *out = config_list;
    return config_list_next;
}

int config_resolve(const char *mod_name, const struct config cfg_map[], size_t len)
{
    for (size_t i = 0; i < len; i++) {
        const struct config * const cfg = &cfg_map[i];
        const char *name = cfg->name;
        const char *str;
        int err;

        str = getenv(name);
        if (!str) {
            continue;
        }

        switch (cfg->type) {
        case CONFIG_CSTRING:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
            *((char **)cfg->dp) = str;
            err = 0;
#pragma GCC diagnostic pop
            break;
        case CONFIG_INT:
            err = parse_int(cfg->dp, str);
            break;
        case CONFIG_SIZE_T:
            err = parse_size_t(cfg->dp, str);
            break;
        }
        if (err) {
            return err;
        }

        SELVA_LOG(SELVA_LOGL_DBG, "Selva config changed: %s", cfg->name);
    }

    config_list_insert(mod_name, cfg_map, len);

    return 0;
}
