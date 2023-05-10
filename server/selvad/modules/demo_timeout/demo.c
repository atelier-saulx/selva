/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "selva_log.h"
#include "event_loop.h"
#include "module.h"

static void my_hello(struct event *, void *)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Hello world from a module");
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import(evl_set_timeout, NULL);
    evl_import_main(evl_clear_timeout);
    // or evl_import_event_loop();
}

__constructor void init(void)
{
    evl_module_init("demo_timeout");

    /*
     * Random timeout.
     */
    struct timespec t1 = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };
    (void)evl_set_timeout(&t1, my_hello, NULL);

    /*
     * Cancelling timeout.
     */
    struct timespec t2 = {
        .tv_sec = 5,
        .tv_nsec = 0,
    };
    int tim = evl_set_timeout(&t2, my_hello, NULL);
    evl_clear_timeout(tim, NULL);
}
