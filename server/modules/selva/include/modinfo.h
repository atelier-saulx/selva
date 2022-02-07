#pragma once
#ifndef SELVA_MODINFO_H
#define SELVA_MODINFO_H

#include "linker_set.h"

struct RedisModuleInfoCtx;

/**
 * Type for module info function.
 */
typedef void SelvaModinfoFn(struct RedisModuleInfoCtx *ctx);

struct SelvaModinfo {
    const char *name;
    SelvaModinfoFn *fn;
};

/**
 * Provide module information.
 */
#define SELVA_MODINFO(_name_, fun)          \
    static const struct SelvaModinfo _modinfo = { \
        .name = (const char * const)_name_, \
        .fn = fun,                          \
    };                                      \
    DATA_SET(selva_modnfo, _modinfo)

#endif /* SELVA_MODINFO_H */
