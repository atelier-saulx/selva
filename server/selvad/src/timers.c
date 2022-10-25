#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "selva_error.h"
#include "ctime.h"
#include "event_loop.h"
#include "timers.h"

#if __MACH__ || __APPLE__
#define TIME_SOURCE CLOCK_MONOTONIC
#elif defined(CLOCK_MONOTONIC_COARSE)
#define TIME_SOURCE CLOCK_MONOTONIC_COARSE
#elif _POSIX_MONOTONIC_CLOCK > 0
#define TIME_SOURCE CLOCK_MONOTONIC
#else
#define TIME_SOURCE CLOCK_REALTIME
#endif

static int timer_cmp(const void *data, heap_value_t a, heap_value_t b)
{
    struct timer_reg *timers = (struct timer_reg *)data;
    struct timespec at = timers[a].timeout;
    struct timespec bt = timers[b].timeout;

    if (timespec_cmp(&bt, &at, <)) {
        return -1;
    } else if (timespec_cmp(&bt, &at, >)) {
        return 1;
    } else {
        return 0;
    }
}

static void get_monotime(struct timespec *spec)
{
    clock_gettime(TIME_SOURCE, spec);
}

void evl_timers_init(struct timers *timers)
{
    SLIST_INIT(&timers->tim_free);
    for (int i = 0; i < EVENT_LOOP_MAX_TIMERS; i++) {
        SLIST_INSERT_HEAD(&timers->tim_free, &timers->tim[i], next_free);
    }

    heap_init(&timers->tim_queue, timers->tim, timer_cmp, EVENT_LOOP_MAX_TIMERS);
}

static int get_tim_id(struct timers *timers, struct timer_reg *tim)
{
    return (intptr_t)(tim - timers->tim);
}

static struct timer_reg *new_tim(struct timers *timers)
{
    struct timer_reg *tim;

    tim = SLIST_FIRST(&timers->tim_free);
    if (!tim) {
        return NULL;
    }

    SLIST_REMOVE_HEAD(&timers->tim_free, next_free);
    memset(&tim->next_free, 0, sizeof(tim->next_free));

    return tim;
}

/**
 * Release the timer back to the free list.
 * The timer must not be in the queue anymore.
 */
static void release_tim(struct timers *timers, struct timer_reg *tim)
{
    memset(tim, 0, sizeof(*tim));
    SLIST_INSERT_HEAD(&timers->tim_free, tim, next_free);
}

int evl_timers_set_timeout(struct timers *timers, const struct timespec * restrict timeout, evl_event_cb cb, void *arg)
{
    struct timespec spec;
    struct timer_reg *tim;
    int tim_id;

    if (!cb) {
        return SELVA_EINVAL;
    }

    tim = new_tim(timers);
    if (!tim) {
        return SELVA_ENOBUFS;
    }

    get_monotime(&spec);
    timespec_add(&tim->timeout, &spec, timeout);
    tim->cb = cb;
    tim->arg = arg;

    tim_id = get_tim_id(timers, tim);
    heap_insert(&timers->tim_queue, tim_id);

    return tim_id;
}

void evl_timers_clear_timeout(struct timers *timers, int tim_id)
{
    struct timer_reg *tim;

    if (tim_id < 0 || tim_id >= EVENT_LOOP_MAX_TIMERS) {
        return;
    }

    tim = &timers->tim[tim_id];
    if (!tim->cb) {
        return;
    }

    tim->timeout.tv_sec = -1;
    tim->timeout.tv_nsec = 0;
    while (heap_peek_max(&timers->tim_queue) != tim_id) {
        tim->timeout.tv_sec--;
        heap_inc_key(&timers->tim_queue, tim_id);
    }

    heap_del_max(&timers->tim_queue);
    release_tim(timers, tim);
}

int evl_timers_nr_waiting(struct timers *timers)
{
    return heap_len(&timers->tim_queue);
}

/**
 * Check if any of the timers has expired and insert the cbs of expired
 * timers into the event loop.
 */
void evl_timers_tick(struct timers *timers)
{
    struct timespec spec;
    int tim_id;

    get_monotime(&spec);

    while ((tim_id = heap_peek_max(&timers->tim_queue)) != -1) {
        struct timer_reg *tim;
        struct event event;

        assert(tim_id >= 0 && tim_id < EVENT_LOOP_MAX_TIMERS);

        tim = &timers->tim[tim_id];
        if (!timespec_cmp(&tim->timeout, &spec, <=)) {
            break;
        }

        heap_del_max(&timers->tim_queue);

        /*
         * Prepare the event struct.
         */
        event.mask = EVENT_TYPE_TIMER;
        event.timer_id = tim_id;

        /*
         * Call the registered callback for this timer.
         */
        tim->cb(&event, tim->arg);

        /*
         * Release the timer struct.
         */
        release_tim(timers, tim);
    }
}

struct timespec *evl_timers_next_timeout(struct timers *timers, struct timespec *spec)
{
    int tim_id;
    struct timer_reg *tim;
    const struct timespec ref = { 0 };
    struct timespec cur;
    struct timespec res;

    tim_id = heap_peek_max(&timers->tim_queue);
    if (tim_id < 0 || tim_id >= EVENT_LOOP_MAX_TIMERS) {
        return NULL;
    }

    tim = &timers->tim[tim_id];

    get_monotime(&cur);
    timespec_sub(&res, &tim->timeout, &cur);
    memcpy(spec, timespec_cmp(&res, &ref, >) ? &res : &ref, sizeof(*spec));

    return spec;
}
