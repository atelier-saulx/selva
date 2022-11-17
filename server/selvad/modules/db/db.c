/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include "jemalloc.h"
#include "libdeflate.h"
#include "linker_set.h"
#include "event_loop.h"
#include "module.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_onload.h"
#include "selva_server.h"
#include "config.h"

SET_DECLARE(selva_onload, Selva_Onload);
SET_DECLARE(selva_onunld, Selva_Onunload);

#if 0
static void test(struct selva_server_response_out *resp, const char *buf __unused, size_t size __unused) {
    const char msg[] = "test";

    selva_send_str(resp, msg, sizeof(msg) - 1);
    server_send_end(resp);
}
#endif

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
    import_selva_server();
}

__constructor void init(void)
{
    int err;
    Selva_Onload **onload_p;

    SELVA_LOG(SELVA_LOGL_INFO, "Init db");

#if 0
    selva_mk_command(2, test);
#endif

    /* FIXME Selva version */
#if 0
    SELVA_LOG(SELVA_LOGL_INFO, "Selva version: %s", selva_version);
#endif

    libdeflate_set_memory_allocator(selva_malloc, selva_free);

    /* FIXME How to pass selva config args */
#if 0
    err = parse_config_args(argv, argc);
    if (err) {
        SELVA_LOG(SELVA_LOGL_CRIT, "Failed to parse config args: %s",
                  selva_strerror(err));
        exit(EXIT_FAILURE);
    }
#endif

    SET_FOREACH(onload_p, selva_onload) {
        Selva_Onload *onload = *onload_p;

        err = onload();
        if (err) {
            SELVA_LOG(SELVA_LOGL_CRIT, "Failed to init db: %s",
                      selva_strerror(err));
            exit(EXIT_FAILURE);
        }
    }
}

__destructor void deinit(void) {
    Selva_Onunload **onunload_p;

    SET_FOREACH(onunload_p, selva_onunld) {
        Selva_Onunload *onunload = *onunload_p;

        onunload();
    }
}
