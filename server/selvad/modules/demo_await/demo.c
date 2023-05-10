/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "selva_log.h"
#include "event_loop.h"
#include "promise.h"
#include "module.h"

static void resolve(struct event *e __unused, void *arg)
{
    struct evl_promise *p = (struct evl_promise *)arg;

    SELVA_LOG(SELVA_LOGL_INFO, "The task is ready");
    evl_promise_resolve(p, NULL);
}

static struct evl_promise *slow_fun(struct evl_async_ctx *ctx)
{
    struct evl_promise *p = evl_promise_new(ctx);
    struct timespec t1 = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };

    SELVA_LOG(SELVA_LOGL_INFO, "The following is going to take at least 1 sec");
    evl_set_timeout(&t1, resolve, p);
    SELVA_LOG(SELVA_LOGL_INFO, "But we continue immediately");

    return p;
}

static void async_fun(struct evl_async_ctx *ctx, void *arg __unused)
{
    evl_async_ctx_auto_cleanup(ctx);
    struct evl_promise *p;
    enum evl_promise_status s;

    SELVA_LOG(SELVA_LOGL_INFO, "In async function");
    p = slow_fun(ctx);
    SELVA_LOG(SELVA_LOGL_INFO, "Going to await p");
    s = evl_promise_await(p, NULL);
    SELVA_LOG(SELVA_LOGL_INFO, "Returned from await with status: %d", s);

    /* Using __auto_cleanup_ctx */
#if 0
    evl_return_from_async(ctx);
#endif
}

/**
 * This is a timer callback that creates an async context to continue its
 * work asynchronously using async-await.
 */
static void async_example(struct event *e __unused, void *arg __unused)
{
    SELVA_LOG(SELVA_LOGL_INFO, "AAA");
    evl_call_async(async_fun, NULL);
    SELVA_LOG(SELVA_LOGL_INFO, "BBB");
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_main(evl_set_timeout);
    evl_import_main(evl_call_async);
    evl_import_promise();
}

__constructor void init(void)
{
    evl_module_init("demo_await");

    struct timespec t = {
        .tv_sec = 0,
        .tv_nsec = 10,
    };
    evl_set_timeout(&t, async_example, NULL);
}
