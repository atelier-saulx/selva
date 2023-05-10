/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "selva_log.h"
#include "event_loop.h"
#include "module.h"

static const char *modules[] = {
    "mod_signal.so",
#if 0
    "mod_demo_timeout.so",
    "mod_demo_async.so",
    "mod_demo_sock.so",
#endif
    "mod_server.so",
    "mod_io.so",
    "mod_replication.so",
    "mod_db.so",
};

int main(void)
{
    evl_module_init("main");
    evl_init();

    SELVA_LOG(SELVA_LOGL_INFO, "Selva build: %s pid: %jd",
              __DATE__, (intmax_t)getpid());

    for (size_t i = 0; i < num_elem(modules); i++) {
        if (!evl_load_module(modules[i])) {
            exit(EXIT_FAILURE);
        }
    }

    evl_start();
    evl_deinit();

    return 0;
}
