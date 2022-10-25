#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "event_loop.h"
#include "promise.h"
#include "module.h"

static void resolve(struct event *e __unused, void *arg)
{
    struct evl_promise *p = (struct evl_promise *)arg;

    printf("%s: The task is ready\n", __func__);
    evl_promise_resolve(p, NULL);
}

static struct evl_promise *slow_fun(struct evl_async_ctx *ctx)
{
    struct evl_promise *p = evl_promise_new(ctx);
    struct timespec t1 = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };

    printf("%s: The following is going to take at least 1 sec\n", __func__);
    evl_set_timeout(&t1, resolve, p);
    printf("%s: But we continue immediately\n", __func__);

    return p;
}

static void async_fun(struct evl_async_ctx *ctx, void *arg __unused)
{
    evl_async_ctx_auto_cleanup(ctx);
    struct evl_promise *p;
    enum evl_promise_status s;

    printf("%s: In async function\n", __func__);
    p = slow_fun(ctx);
    printf("%s: Going to await p\n", __func__);
    s = evl_promise_await(p, NULL);
    printf("%s: Returned from await with status: %d\n", __func__, s);

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
    printf("AAA\n");
    evl_call_async(async_fun, NULL);
    printf("BBB\n");
}

__constructor void init(void)
{
    printf("Init demo_await\n");

    evl_import_main(evl_set_timeout);
    evl_import_main(evl_call_async);
    evl_import_promise();

    struct timespec t = {
        .tv_sec = 0,
        .tv_nsec = 10,
    };
    evl_set_timeout(&t, async_example, NULL);
}
