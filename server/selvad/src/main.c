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

    evl_load_module("mod_signal.so");
#if 0
    evl_load_module("mod_demo_timeout.so");
    evl_load_module("mod_demo_async.so");
    evl_load_module("mod_demo_sock.so");
#endif
    evl_load_module("mod_server.so");
    evl_load_module("mod_replication.so");
    evl_load_module("mod_io.so");
    evl_load_module("mod_db.so");

    evl_start();
    evl_deinit();

    return 0;
}
