#include <stdlib.h>
#include "cdefs.h"
#include "sds.h"

struct RedisModuleCtx;
struct RedisModuleString;
struct redisObjectAccessor {
    uint32_t _meta;
    int refcount;
    void *ptr;
};

void *_RedisModule_Alloc(size_t size) {
    return calloc(1, size);
}

void *_RedisModule_Calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *_RedisModule_Realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void _RedisModule_Free(void *ptr) {
    free(ptr);
}

static struct RedisModuleString *_RedisModule_CreateString(struct RedisModuleCtx *ctx __unused, const char *ptr, size_t len) {
	struct redisObjectAccessor *robj;

    robj = calloc(1, sizeof(*robj));
    if (!robj) {
        abort();
    }

    robj->refcount = 1;
    robj->ptr = sdsnewlen(ptr, len);
    if (!robj->ptr) {
        abort();
    }

    return (struct RedisModuleString *)robj;
}

/*
 * Partilally copied from redis-server module.c
 */
static const char *_RedisModule_StringPtrLen(struct RedisModuleString *str, size_t *len) {
    struct redisObjectAccessor *robj = (struct redisObjectAccessor *)str;

	if (!str) {
		static const char errmsg[] = "(NULL string reply referenced in module)";

		if (len) {
            *len = sizeof(errmsg) - 1;
        }

		return errmsg;
	}

	if (len) {
        *len = sdslen(robj->ptr);
    }

	return robj->ptr;
}

static void _RedisModule_FreeString(struct RedisModuleCtx *ctx __unused, struct RedisModuleString *str) {
    struct redisObjectAccessor *robj = (struct redisObjectAccessor *)str;

    if (str && --robj->refcount == 0) {
        sdsfree(robj->ptr);
        free(robj);
    }
}

static void _RedisModule_RetainString(struct RedisModuleCtx *ctx __unused, struct RedisModuleString *str) {
    struct redisObjectAccessor *robj = (struct redisObjectAccessor *)str;

    robj->refcount++;
}

extern void * (*RedisModule_Alloc)(size_t size) = _RedisModule_Alloc;
extern void* (*RedisModule_Calloc)(size_t nmemb, size_t size) = _RedisModule_Calloc;
extern void * (*RedisModule_Realloc)(void *ptr, size_t size) = _RedisModule_Realloc;
extern void (*RedisModule_Free)(void *ptr) = _RedisModule_Free;
extern struct RedisModuleString *(*RedisModule_CreateString) = _RedisModule_CreateString;
extern const char *(*RedisModule_StringPtrLen)(struct RedisModuleString *str, size_t *len) = _RedisModule_StringPtrLen;
extern void (*RedisModule_FreeString)(struct RedisModuleCtx *ctx __unused, struct RedisModuleString *str) = _RedisModule_FreeString;
extern void (*RedisModule_RetainString)(struct RedisModuleCtx *ctx __unused, struct RedisModuleString *str) =  _RedisModule_RetainString;
