/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <string.h>
#include "redismodule.h"
#include "linker_set.h"
#include "selva.h"

const char * const selvaStrError[-SELVA_INVALID_ERROR + 1] = {
    [0]                                 = (const char *)"ERR_SELVA No Error",
    [-SELVA_EGENERAL]                   = (const char *)"ERR_SELVA EGENERAL Unknown error",
    [-SELVA_ENOTSUP]                    = (const char *)"ERR_SELVA ENOTSUP Operation not supported",
    [-SELVA_EINVAL]                     = (const char *)"ERR_SELVA EINVAL Invalid argument or input value",
    [-SELVA_EINTYPE]                    = (const char *)"ERR_SELVA EINTYPE Invalid type",
    [-SELVA_ENAMETOOLONG]               = (const char *)"ERR_SELVA ENAMETOOLONG Name too long",
    [-SELVA_ENOMEM]                     = (const char *)"ERR_SELVA ENOMEM Out of memory",
    [-SELVA_ENOENT]                     = (const char *)"ERR_SELVA ENOENT Not found",
    [-SELVA_EEXIST]                     = (const char *)"ERR_SELVA EEXIST Exist",
    [-SELVA_ENOBUFS]                    = (const char *)"ERR_SELVA ENOBUFS No buffer or resource space available",
    [-SELVA_HIERARCHY_EGENERAL]         = (const char *)"ERR_HIERARCHY EGENERAL Unknown error",
    [-SELVA_HIERARCHY_ENOTSUP]          = (const char *)"ERR_HIERARCHY ENOTSUP Operation not supported",
    [-SELVA_HIERARCHY_EINVAL]           = (const char *)"ERR_HIERARCHY EINVAL Invalid argument or input value",
    [-SELVA_HIERARCHY_ENOMEM]           = (const char *)"ERR_HIERARCHY ENOMEM Out of memory",
    [-SELVA_HIERARCHY_ENOENT]           = (const char *)"ERR_HIERARCHY ENOENT Not found",
    [-SELVA_HIERARCHY_EEXIST]           = (const char *)"ERR_HIERARCHY EEXIST Exist",
    [-SELVA_HIERARCHY_ETRMAX]           = (const char *)"ERR_HIERARCHY ETRMAX Maximum number of recursive find calls reached",
    [-SELVA_SUBSCRIPTIONS_EGENERAL]     = (const char *)"ERR_SUBSCRIPTIONS EGENERAL Unknown error",
    [-SELVA_SUBSCRIPTIONS_EINVAL]       = (const char *)"ERR_SUBSCRIPTIONS EINVAL Invalid argument or input value",
    [-SELVA_SUBSCRIPTIONS_ENOMEM]       = (const char *)"ERR_SUBSCRIPTIONS ENOMEM Out of memory",
    [-SELVA_SUBSCRIPTIONS_ENOENT]       = (const char *)"ERR_SUBSCRIPTIONS ENOENT Not found",
    [-SELVA_SUBSCRIPTIONS_EEXIST]       = (const char *)"ERR_SUBSCRIPTIONS EEXIST Exist",
    [-SELVA_RPN_ECOMP]                  = (const char *)"ERR_RPN ECOMP Expression compilation failed",
    [-SELVA_OBJECT_EOBIG]               = (const char *)"ERROR_SELVA_OBJECT Maximum number of keys reached",
    [-SELVA_INVALID_ERROR]              = (const char *)"ERR_SELVA Invalid error code"
};

const char *getSelvaErrorStr(int err) {
    if (err > 0 || -err >= (int)num_elem(selvaStrError)) {
        return selvaStrError[-SELVA_EGENERAL];
    }
    return selvaStrError[-err];
}

/* declared as weak so the unit tests can override it. */
__weak_sym int replyWithSelvaError(RedisModuleCtx *ctx, int err) {
    return RedisModule_ReplyWithError(ctx, getSelvaErrorStr(err));
}

__weak_sym int replyWithSelvaErrorf(RedisModuleCtx *ctx, int selvaErr, const char *fmt, ...) {
    va_list args;
    char buf[512];
    const char *msg;
    size_t len;
    int err;

    va_start(args, fmt);
    msg = getSelvaErrorStr(selvaErr);
    len = strlen(msg);

    if (len + 2 <= sizeof(buf) - 1) {
        strcpy(buf, msg);

        buf[len++] = ':';
        buf[len++] = ' ';
        vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
        msg = buf;
    }

    err = RedisModule_ReplyWithError(ctx, msg);

    va_end(args);
    return err;
}
