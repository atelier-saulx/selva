/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "selva_log.h"
#include "event_loop.h"
#include "module.h"

int main(void)
{
    evl_init();

    /* TODO Fail if mod load fails */
    SELVA_LOG(SELVA_LOGL_INFO, "Selva %s", __DATE__);
    evl_load_module("modules/signal.so");
#if 0
    evl_load_module("modules/demo_timeout.so");
    evl_load_module("modules/demo_async.so");
    evl_load_module("modules/demo_sock.so");
#endif
    evl_load_module("modules/server.so");
    evl_load_module("modules/db.so");

    evl_start();
    evl_deinit();

    return 0;
}
