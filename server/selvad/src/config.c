#include <stddef.h>
#include <stdlib.h>
#include "selva_log.h"
#include "selva_error.h"
#include "config.h"

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

int config_resolve(const struct config cfg_map[], size_t len)
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
            *((char **)cfg->dp) = str;
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

        SELVA_LOG_DBG("Selva config changed: %s\n", cfg->name);
    }

    /* TODO Add to the global config map */

    return 0;
}

/* FIXME Implement SELVA_MODINFO */
#if 0
static void mod_info(RedisModuleInfoCtx *ctx)
{
    for (size_t i = 0; i < num_elem(cfg_map); i++) {
        struct cfg const * const cfg = &cfg_map[i];
        char *name = (char *)cfg->name;

        if (cfg->parse == &parse_size_t) {
            (void)RedisModule_InfoAddFieldULongLong(ctx, name, *(size_t *)cfg->dp);
        } else if (cfg->parse == &parse_int) {
            (void)RedisModule_InfoAddFieldLongLong(ctx, name, *(int *)cfg->dp);
        } else {
            (void)RedisModule_InfoAddFieldCString(ctx, name, "Unsupported type");
        }
    }
}
SELVA_MODINFO("config", mod_info);
#endif
