#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include "event_loop_state.h"
#include "promise.h"

#if __linux__
#define ASYNC_STACK_MMAP_FLAGS \
    (MAP_ANONYMOUS | MAP_PRIVATE | MAP_STACK | MAP_GROWSDOWN)
#elif __APPLE__
#define ASYNC_STACK_MMAP_FLAGS \
    MAP_ANONYMOUS | MAP_PRIVATE
#else
#error "Unsupported target system"
#endif

/**
 * Breakup a 64-bit pointer into two 32-bit integers.
 * Note that MSB remains 0 so that the integer can be casted
 * properly.
 */
#define BREAKUP_POINTER(_p_) \
    (int)(((uintptr_t)(_p_)) >> 33), (int)((((uintptr_t)(_p_) & 0x1FFFFFFFF) >> 2))

/**
 * Reconstruct a 64-bit pointer from two 32-bit integers.
 */
#define RECONSTRUCT_POINTER(_msw_, _lsw_) \
    ((void *)(((uintptr_t)(_msw_) << 33) | ((uintptr_t)(_lsw_) << 2)))

/**
 * Async function context.
 * Async call stack must have a pointer to this struct to utilize
 * promises.
 */
struct evl_async_ctx {
    ucontext_t async_uctx;
    char *stack;
    async_func start;
    void *arg; /*!< Argument for start(). */

    /**
     * Alloc list entry pointers.
     * Possible lists:
     * - async_ctx_free
     * - async_ctx_busy
     */
    LIST_ENTRY(evl_async_ctx) entries;
};

enum evl_promise_flags {
    /**
     * Awaiting has started.
     */
    EVL_PROMISE_FLAG_AWAITING = 0x01,
    /**
     * Await has ended.
     */
    EVL_PROMISE_FLAG_AWAITED  = 0x02,
    /**
     * Resolve function was called for this promise.
     */
    EVL_PROMISE_FLAG_RESOLVED = 0x04,
    /**
     * Cancel function was called for this promise.
     * This flag is technically redundant but it tells if cancel was called
     * after the promise was settled.
     */
    EVL_PROMISE_FLAG_CANCELLED = 0x08,
} __packed;

struct evl_promise {
    enum evl_promise_status status;
    enum evl_promise_flags flags;
    struct evl_async_ctx *ctx;
    void *res; /*!< Result passed to evl_promise_resolve(). */

    /**
     * List entry pointer(s).
     * Possible lists:
     * - async_settled_promises
     */
    SLIST_ENTRY(evl_promise) entries;
};

static struct evl_async_ctx *get_async_ctx(void)
{
    struct evl_async_ctx *ctx;

    ctx = LIST_FIRST(&event_loop_state.async_ctx_free);
    if (!ctx) {
        return NULL;
    }

    LIST_REMOVE(ctx, entries);
    LIST_INSERT_HEAD(&event_loop_state.async_ctx_busy, ctx, entries);

    return ctx;
}

static void release_async_ctx(struct evl_async_ctx *ctx)
{
    LIST_REMOVE(ctx, entries);
    LIST_INSERT_HEAD(&event_loop_state.async_ctx_free, ctx, entries);
}

/**
 * Trampoline to call async functions with makecontext().
 * This trampoline is used to pass 64-bit pointer using two (32-bit) int
 * arguments used on many platforms supporting makecontext(). It's not
 * necessary to use this trampoline on x86-64 glibc >2.8 because it allows
 * passing 64-bit integers as arguments.
 */
__used static void async_trampoline(int ctx_msw, int ctx_lsw)
{
    struct evl_async_ctx *ctx = RECONSTRUCT_POINTER(ctx_msw, ctx_lsw);
    async_func start = ctx->start;

    ctx->start = NULL; /* Make sure the context will never restart. */
    start(ctx, ctx->arg);
}

void evl_call_async(async_func afun, void *arg)
{
    struct evl_async_ctx *ctx = get_async_ctx();

    if (!ctx) {
        /* TODO Better error log */
        fprintf(stderr, "no more free contexts");
        exit(EXIT_FAILURE);
    }

    if (getcontext(&ctx->async_uctx)) {
        /* TODO Better error log */
        fprintf(stderr, "getcontext failed");
        exit(EXIT_FAILURE);
    }

    ctx->async_uctx.uc_stack.ss_sp = ctx->stack;
    ctx->async_uctx.uc_stack.ss_size = EVENT_LOOP_ASYNC_STACK_SIZE;
    ctx->async_uctx.uc_link = &event_loop_state.async_uctx_main;
    ctx->start = afun;
    ctx->arg = arg;

    /*
     * Glibc version >= 2.8 on x86-64 passes arguments to makecontext() as 64-bit
     * integers and thus mangling pointers is unnecessary.
     */
#if defined(__x86_64__) && __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
    makecontext(&ctx->async_uctx, (void (*)())afun, 1, ctx);
#else
    makecontext(&ctx->async_uctx, (void (*)())async_trampoline, 2, BREAKUP_POINTER(ctx));
#endif

    if (swapcontext(&event_loop_state.async_uctx_main, &ctx->async_uctx)) {
        /* TODO Better error log */
        fprintf(stderr, "swapcontext failed");
        abort();
    }
}

void evl_return_from_async(struct evl_async_ctx *ctx)
{
    /*
     * This could be automated with another trampoline and spare context but
     * it might be cleaner to just require the async functions to call this.
     */
    release_async_ctx(ctx);
}

struct evl_promise *evl_promise_new(struct evl_async_ctx *ctx)
{
    struct evl_promise *p = calloc(1, sizeof(struct evl_promise));
    p->status = EVL_PROMISE_STATUS_PENDING;
    p->ctx = ctx;
    return p;
}

enum evl_promise_status evl_promise_await(struct evl_promise *p, void **res)
{
    enum evl_promise_status s;

    if (p->flags & (EVL_PROMISE_FLAG_AWAITING | EVL_PROMISE_FLAG_AWAITED)) {
        /* A promise can be only awaited once. */
        /* TODO Better error log */
        fprintf(stderr, "double await");
        abort(); /* Is abort() the right way? */
    }

    p->flags |= EVL_PROMISE_FLAG_AWAITING;
    event_loop_state.async_nr_awaiting++;
    while (p->status == EVL_PROMISE_STATUS_PENDING) {
        swapcontext(&p->ctx->async_uctx, &event_loop_state.async_uctx_main);
    }
    event_loop_state.async_nr_awaiting--;
    p->flags &= ~EVL_PROMISE_FLAG_AWAITING;
    p->flags |= EVL_PROMISE_FLAG_AWAITED;

    s = p->status;
    if (res) {
        *res = p->res;
    }

    /*
     * We can only free the promise if we know that nobody
     * is going to touch it anymore.
     */
    if (p->flags & EVL_PROMISE_FLAG_RESOLVED) {
        memset(p, 0, sizeof(*p));
        free(p);
    }

    return s;
}

void evl_promise_resolve(struct evl_promise *p, void *res)
{
    p->flags |= EVL_PROMISE_FLAG_RESOLVED;

    if (p->status == EVL_PROMISE_STATUS_PENDING) {
        p->status = EVL_PROMISE_STATUS_FULFILLED;
        p->res = res;
        SLIST_INSERT_HEAD(&event_loop_state.async_settled_promises, p, entries);
    } else if (p->status == EVL_PROMISE_STATUS_CANCELLED &&
               (p->flags & EVL_PROMISE_FLAG_AWAITED)) {
        /*
         * Free the promise if it was already awaited by the owner.
         */
        memset(p, 0, sizeof(*p));
        free(p);
    }
}

void evl_promise_cancel(struct evl_promise *p)
{
    p->flags |= EVL_PROMISE_FLAG_CANCELLED;

    if (p->status == EVL_PROMISE_STATUS_PENDING) {
        p->status = EVL_PROMISE_STATUS_CANCELLED;
        p->res = NULL;
        SLIST_INSERT_HEAD(&event_loop_state.async_settled_promises, p, entries);
    }
}

void event_loop_init_promises(void)
{
    LIST_INIT(&event_loop_state.async_ctx_free);
    LIST_INIT(&event_loop_state.async_ctx_busy);

    /*
     * Create contexts for async functions.
     */
    for (int i = 0; i < EVENT_LOOP_MAX_ASYNC; i++) {
        struct evl_async_ctx *ctx;

        ctx = calloc(1, sizeof(*ctx));
        ctx->stack = mmap(NULL, EVENT_LOOP_ASYNC_STACK_SIZE,
                          PROT_READ | PROT_WRITE,
                          ASYNC_STACK_MMAP_FLAGS,
                          -1, 0);

        LIST_INSERT_HEAD(&event_loop_state.async_ctx_free, ctx, entries);
    }

    SLIST_INIT(&event_loop_state.async_settled_promises);
}

void event_loop_handle_settled_promises(void)
{
    struct evl_promise *p;
    struct evl_promise *p_temp;

    SLIST_FOREACH_SAFE(p, &event_loop_state.async_settled_promises, entries, p_temp) {
        if (p->status != EVL_PROMISE_STATUS_PENDING &&
            (p->flags & EVL_PROMISE_FLAG_AWAITING)) {
            /*
             * Note that at this point if the status of the promise was
             * EVL_PROMISE_STATUS_CANCELLED we'll lose track of it within
             * the event_loop and we can only hope that some async function
             * will call `evl_promise_resolve()` for it.
             */
            SLIST_REMOVE_HEAD(&event_loop_state.async_settled_promises, entries);
            swapcontext(&event_loop_state.async_uctx_main, &p->ctx->async_uctx);
        }
    }
}
