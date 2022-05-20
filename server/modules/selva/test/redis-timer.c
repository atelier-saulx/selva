#include "redismodule.h"

RedisModuleTimerID _RedisModule_CreateTimer(RedisModuleCtx *ctx, mstime_t period, RedisModuleTimerProc callback, void *data) {
    return -1;
}

int _RedisModule_StopTimerUnsafe(RedisModuleTimerID id, void **data) {
    return 0;
}

RedisModuleTimerID (*RedisModule_CreateTimer)(RedisModuleCtx *ctx, mstime_t period, RedisModuleTimerProc callback, void *data) = _RedisModule_CreateTimer;
int (*RedisModule_StopTimerUnsafe)(RedisModuleTimerID id, void **data) = _RedisModule_StopTimerUnsafe;
