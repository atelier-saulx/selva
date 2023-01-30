/*
 * TODO This should be moved to util or the main.
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE
#include <pthread.h>
#include "selva_error.h"

/* TODO Macos support */

/*
 * TODO We need a global config for core mapping.
 */
int selva_thread_self_set_core(int core_id)
{
    const pthread_t thread = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
        /* TODO Better error handling. */
        return SELVA_EGENERAL;
    }

    return 0;
}
