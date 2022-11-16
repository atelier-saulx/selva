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
#include "selva_server.h"

static void test(struct selva_server_response_out *resp, const char *buf __unused, size_t size __unused) {
    const char msg[] = "test";

    selva_send_str(resp, msg, sizeof(msg) - 1);
    server_send_end(resp);
}

IMPORT() {
    evl_import_main(selva_log);
    evl_import_event_loop();
    import_selva_server();
}

__constructor void init(void)
{
    SELVA_LOG(SELVA_LOGL_INFO, "Init db");

    selva_mk_command(2, test);
}
