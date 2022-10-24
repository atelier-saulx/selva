#pragma once

#include "_evl_export.h"

/**
 * Promise status.
 */
enum evl_promise_status {
    EVL_PROMISE_STATUS_PENDING,
    EVL_PROMISE_STATUS_FULFILLED,
    EVL_PROMISE_STATUS_CANCELLED,
} __packed;

struct evl_async_ctx;
struct evl_promise;

/**
 * Async function.
 * This function type isn't asynchronous itself but promises can be created and
 * awaited within this type of function.
 */
typedef void (*async_func)(struct evl_async_ctx *ctx, void *arg);

/**
 * Call an async function that requires evl_async_ctx.
 * This function should be only called when the process is executing using the
 * main stack and there is no evl_async_ctx available in the scope.
 */
EVL_EXPORT(void, evl_call_async, async_func afun, void *arg);

/**
 * Return from async context.
 * This is an alternative manual cleanup for __auto_cleanup_ctx.
 */
EVL_EXPORT(void, evl_return_from_async, struct evl_async_ctx *ctx);

static inline void _evl_auto_cleanup_async(void *p)
{
    struct evl_async_ctx *ctx = *(struct evl_async_ctx **)p;

    evl_return_from_async(ctx);
}

/**
 * Clean up the async execution context automatically when the scope closes.
 * This should be used in the beginning of the first function receiving an async
 * context, i.e. the callback of evl_call_async().
 * ```c
 * evl_async_ctx_auto_cleanup(ctx);
 * ```
 */
#define evl_async_ctx_auto_cleanup(_ctx_) __attribute__((__cleanup__(_evl_auto_cleanup_async), unused)) struct evl_async_ctx *_ ## _ctx_ = _ctx_

/**
 * Create a new promise.
 * Promises can be only created within the call stack of an
 * async_func.
 */
EVL_EXPORT(struct evl_promise *, evl_promise_new, struct evl_async_ctx *ctx);

/**
 * Wait for the promise p to be resolved.
 * Calling await for the same promise more than once is an error.
 * The promise `p` is typically freed immediately once this
 * function returns.
 */
EVL_EXPORT(enum evl_promise_status, evl_promise_await, struct evl_promise *p, void **res);

/**
 * Mark a promise fulfilled.
 * Calling resolve for the same promise more than once is an error.
 */
EVL_EXPORT(void, evl_promise_resolve, struct evl_promise *p, void *res);

/**
 * Mark a promise cancelled.
 * Calling cancel for the same promise more than once is an error.
 * However, it's legal to call `evl_promise_resolve()` for the same promise `p`
 * after calling this function.
 */
EVL_EXPORT(void, evl_promise_cancel, struct evl_promise *p);

#define _evl_import_promise(apply) \
    apply(evl_call_async) \
    apply(evl_return_from_async) \
    apply(evl_promise_new) \
    apply(evl_promise_await) \
    apply(evl_promise_resolve) \
    apply(evl_promise_cancel)

/**
 * Import all symbols from promise.h.
 */
#define evl_import_promise() \
    _evl_import_promise(evl_import_main)
