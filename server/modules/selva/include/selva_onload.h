#pragma once
#ifndef SELVA_ONLOAD_H
#define SELVA_ONLOAD_H

#include "linker_set.h"

struct RedisModuleCtx;

typedef int Selva_Onload(struct RedisModuleCtx *ctx);
typedef int Selva_Onunload(void);

#define SELVA_ONLOAD(fun) \
    DATA_SET(selva_onload, fun)

#define SELVA_ONUNLOAD(fun) \
    DATA_SET(selva_onunld, fun)

#endif /* SELVA_ONLOAD_H */
