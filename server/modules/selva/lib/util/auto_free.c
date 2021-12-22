#include "redismodule.h"
#include "auto_free.h"

void _wrapFree(void *p) {
    void **pp = (void **)p;

    RedisModule_Free(*pp);
}

