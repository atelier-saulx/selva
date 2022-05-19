#pragma once
#ifndef SELVA_ONLOAD_H
#define SELVA_ONLOAD_H

#include "linker_set.h"

struct RedisModuleCtx;

typedef int Selva_Onload(struct RedisModuleCtx *ctx); /*!< Onload function. */
typedef void Selva_Onunload(void); /*!< Onunload function. */

/**
 * Run fun when the module is loading.
 */
#define SELVA_ONLOAD(fun) \
    DATA_SET(selva_onload, fun)

/**
 * Run fun when the module is unloading.
 */
#define SELVA_ONUNLOAD(fun) \
    DATA_SET(selva_onunld, fun)

#endif /* SELVA_ONLOAD_H */
