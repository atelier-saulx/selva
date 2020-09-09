#pragma once
#ifndef SELVA_ONLOAD_H
#define SELVA_ONLOAD_H

#include "linker_set.h"

struct RedisModuleCtx;

typedef int Selva_Onload(struct RedisModuleCtx *ctx);

#define SELVA_ONLOAD(fun) \
    DATA_SET(selva_onload, fun)

#endif /* SELVA_ONLOAD_H */
