#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "trx.h"

#define timespec_cmp(tvp, uvp, cmp)             \
    (((tvp)->tv_sec == (uvp)->tv_sec)           \
        ? ((tvp)->tv_nsec cmp (uvp)->tv_nsec)   \
        : ((tvp)->tv_sec cmp (uvp)->tv_sec))

void Trx_Begin(Trx *trx) {
    assert(trx->tv_sec == 0 && trx->tv_nsec == 0);

#ifdef __MACH__
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);

	trx->tv_sec = mts.tv_sec;
	trx->tv_nsec = mts.tv_nsec;
#elif !defined _POSIX_MONOTONIC_CLOCK || _POSIX_MONOTONIC_CLOCK < 0
    clock_gettime(CLOCK_REALTIME, trx);
#elif _POSIX_MONOTONIC_CLOCK > 0
    clock_gettime(CLOCK_MONOTONIC, trx);
#elif _POSIX_TIMERS
    if (clock_gettime(CLOCK_MONOTONIC, trx))
        clock_gettime(CLOCK_REALTIME, trx);
#else
#error No clock source available
#endif
}

void Trx_Stamp(const Trx *trx, struct timespec *ts) {
    memcpy(ts, trx, sizeof(Trx));
}

int Trx_IsStamped(const Trx *trx, struct timespec *ts) {
    return timespec_cmp(trx, ts, ==);
}

void Trx_End(Trx *trx) {
    memset(trx, 0, sizeof(*trx));
}
