#pragma once
#ifndef SELVA_ONLOAD_H
#define SELVA_ONLOAD_H

#include "linker_set.h"

struct RedisModuleCtx;

typedef int SelvaModify_Onload(struct RedisModuleCtx *ctx);

#define SELVA_MODIFY_ONLOAD(fun) \
    DATA_SET(selva_Onload, fun)

#endif /* SELVA_ONLOAD_H */
