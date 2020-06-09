#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redis-rdb.h"

RedisModuleIO *RedisRdb_NewIo(void) {
    RedisModuleIO *io = calloc(1, sizeof(RedisModuleIO));
    io->type = REDIS_MODULE_IO_TYPE_HEAD;

    return io;
}

static RedisModuleIO *RedisRdb_NewIoNode(RedisModuleIO *io, size_t stringSize) {
    RedisModuleIO *node = calloc(1, sizeof(RedisModuleIO) + stringSize);

    if (!node) {
        fprintf(stderr, "malloc() failed\n");
        abort();
    }

    node->next = io->next;
    io->next = node;

    return node;
}

void RedisRdb_FreeIo(RedisModuleIO *io) {
    RedisModuleIO *node;
    RedisModuleIO *next = io;

    while((node = next)) {
        next = node->next;
        free(node);
    }
}

void _RedisModule_SaveUnsigned(RedisModuleIO *io, uint64_t value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_UINT64;
    node->uint64_val = value;
}

uint64_t _RedisModule_LoadUnsigned(RedisModuleIO *io) {
}

void _RedisModule_SaveSigned(RedisModuleIO *io, int64_t value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_INT64;
    node->int64_val = value;
}

int64_t _RedisModule_LoadSigned(RedisModuleIO *io) {
    /* TODO */
}

#if 0
void RedisModule_SaveString(RedisModuleIO *io, RedisModuleString *s) {
    /* TODO */
}
#endif

void _RedisModule_SaveStringBuffer(RedisModuleIO *io, const char *str, size_t len) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, len + 1);

    node->type = REDIS_MODULE_IO_TYPE_STRING;
    memcpy(node->string, str, len);
}

#if 0
RedisModuleString *RedisModule_LoadString(RedisModuleIO *io) {
    /* TODO */
}
#endif

char *_RedisModule_LoadStringBuffer(RedisModuleIO *io, size_t *lenptr) {
    /* TODO */
}

void _RedisModule_SaveDouble(RedisModuleIO *io, double value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_DOUBLE;
    node->double_val = value;
}

double _RedisModule_LoadDouble(RedisModuleIO *io) {
    /* TODO */
}

void (*RedisModule_SaveUnsigned)(RedisModuleIO *io, uint64_t value) = _RedisModule_SaveUnsigned;
uint64_t (*RedisModule_LoadUnsigned)(RedisModuleIO *io) = _RedisModule_LoadUnsigned;
void (*RedisModule_SaveSigned)(RedisModuleIO *io, int64_t value) = _RedisModule_SaveSigned;
int64_t (*RedisModule_LoadSigned)(RedisModuleIO *io) = _RedisModule_LoadSigned;
#if 0
void (*RedisModule_SaveString)(RedisModuleIO *io, RedisModuleString *s) = _RedisModule_SaveString;
#endif
void (*RedisModule_SaveStringBuffer)(RedisModuleIO *io, const char *str, size_t len) = _RedisModule_SaveStringBuffer;
#if 0
RedisModuleString *(*RedisModule_LoadString)(RedisModuleIO *io) = _RedisModule_LoadString;
#endif
char *(*RedisModule_LoadStringBuffer)(RedisModuleIO *io, size_t *lenptr) = _RedisModule_LoadStringBuffer;
void (*RedisModule_SaveDouble)(RedisModuleIO *io, double value) = _RedisModule_SaveDouble;
double (*RedisModule_LoadDouble)(RedisModuleIO *io) = _RedisModule_LoadDouble;
