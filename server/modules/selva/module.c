#include "redismodule.h"
#include "jemalloc.h"
#include "cdefs.h"
#include "libdeflate.h"
#include "config.h"
#include "selva.h"
#include "errors.h"
#include "selva_onload.h"

SET_DECLARE(selva_onload, Selva_Onload);
SET_DECLARE(selva_onunld, Selva_Onunload);

/*
 * This might be useful in the future.
 */
#if 0
static int my_RedisModuleEventCallback(RedisModuleCtx *ctx, RedisModuleEvent eid, uint64_t subevent, void *data) {
    return 0;
}
    (void)RedisModule_SubscribeToServerEvent(ctx, RedisModuleEvent_Shutdown, my_RedisModuleEventCallback);
#endif

SELVA_EXPORT int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    fprintf(stderr, "Selva version: %s\n", selva_version);

    libdeflate_set_memory_allocator(selva_malloc, selva_free);

    /* Register the module itself */
    if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    /*
     * This mode is currently not supported by Selva and should not be enabled
     * as it will just ignore all errors and make Redis crash.
     */
#if 0
    RedisModule_SetModuleOptions(ctx, REDISMODULE_OPTIONS_HANDLE_IO_ERRORS);
#endif

    err = parse_config_args(argv, argc);
    if (err) {
        fprintf(stderr, "%s:%d:%s: Failed to parse config args: %s\n",
                __FILE__, __LINE__, __func__,
                getSelvaErrorStr(err));
        return REDISMODULE_ERR;
    }

    Selva_Onload **onload_p;

    SET_FOREACH(onload_p, selva_onload) {
        Selva_Onload *onload = *onload_p;

        err = onload(ctx);
        if (err) {
            return err;
        }
    }

    return REDISMODULE_OK;
}

/*
 * Here we could use RedisModule_OnUnload() if it was called on exit, but it
 * isn't. Therefore, we use the destructor attribute that is almost always
 * called before the process terminates. As a side note, OnUnload would be never
 * called for Selva because Redis can't unload modules exporting types or
 * something.
 */
__attribute__((destructor))
int _Selva_OnUnload(void) {
    Selva_Onunload **onunload_p;

    SET_FOREACH(onunload_p, selva_onunld) {
        Selva_Onunload *onunload = *onunload_p;

        onunload();
    }

    return REDISMODULE_OK;
}
