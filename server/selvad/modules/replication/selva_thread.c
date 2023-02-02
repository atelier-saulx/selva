/*
 * TODO This should be moved to util or the main.
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <pthread.h>
#if __linux__
#include <sys/sysinfo.h>
#endif
#include "selva_error.h"

/* TODO Macos support */

int selva_thread_get_ncores()
{
#if __linux__
    return get_nprocs();
#else
    return 1;
#endif
}


/*
 * TODO We need a global config for core mapping.
 */
int selva_thread_self_set_core(int core_id)
{
#if __linux__
    const pthread_t thread = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
        /* TODO Better error handling. */
        return SELVA_EGENERAL;
    }
#endif

    return 0;
}
