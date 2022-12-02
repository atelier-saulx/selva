/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "selva_onload.h"
#include "modinfo.h"

SET_DECLARE(selva_modnfo, const struct SelvaModinfo);

static void modinfo(struct selva_server_response_out *resp, const void *buf, size_t len) {
    const struct SelvaModinfo **modnfo_p;

    if (RedisModule_InfoAddSection(ctx, "selva") == REDISMODULE_ERR) {
        return;
    }

    SET_FOREACH(modnfo_p, selva_modnfo) {
        const struct SelvaModinfo *modnfo = *modnfo_p;

        RedisModule_InfoBeginDictField(ctx, (char *)modnfo->name);
        modnfo->fn(ctx);
        RedisModule_InfoEndDictField(ctx);
    }
}

static int Modinfo_OnLoad(void) {
    RedisModule_RegisterInfoFunc(ctx, modinfo);

    return REDISMODULE_OK;
}
SELVA_ONLOAD(Modinfo_OnLoad);
