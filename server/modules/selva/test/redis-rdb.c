#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redis-rdb.h"
#include "cdefs.h"

RedisModuleIO *RedisRdb_NewIo(void) {
    RedisModuleIO *io = calloc(1, sizeof(RedisModuleIO));
    io->type = REDIS_MODULE_IO_TYPE_HEAD;
    io->last = io;

    return io;
}

static RedisModuleIO *RedisRdb_NewIoNode(RedisModuleIO *io, size_t stringSize) {
    RedisModuleIO *node = calloc(1, sizeof(RedisModuleIO) + stringSize);

    if (!node) {
        fprintf(stderr, "malloc() failed\n");
        abort();
    }

    if (io->last) {
        io->last->next = node;
    }
    io->last = node;

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

size_t RedisRdb_CountIo(RedisModuleIO *io) {
    size_t i = 0;
    RedisModuleIO *next = io;

    if (!io) {
        return 0;
    }

    while ((next = next->next)) {
        i++;
    }

    return i;
}

void RedisRdb_Print(RedisModuleIO *io) {
    RedisModuleIO *next = io;
    size_t i = 0;

    while ((next = next->next)) {
        switch (next->type) {
        case REDIS_MODULE_IO_TYPE_UINT64:
            fprintf(stderr, "[%zu] uint64: %" PRIu64 "\n", i, next->uint64_val);
            break;
        case REDIS_MODULE_IO_TYPE_INT64:
            fprintf(stderr, "[%zu] int64: %" PRId64 "\n", i, next->int64_val);
            break;
        case REDIS_MODULE_IO_TYPE_DOUBLE:
            fprintf(stderr, "[%zu] double: %f\n", i, next->double_val);
            break;
        case REDIS_MODULE_IO_TYPE_STRING:
            fprintf(stderr, "[%zu] string: %s\n", i, next->string);
            break;
        default:
            fprintf(stderr, "Invalid RDB type\n");
            abort();
        }
        i++;
    }
}

static RedisModuleIO *RedisRdb_RemoveFirst(RedisModuleIO *io) {
    RedisModuleIO *node = io->next;

    if (node == io->last) {
        io->last = NULL;
    }

    if (node) {
        io->next = node->next;
    }

    return node;
}

void _RedisModule_SaveUnsigned(RedisModuleIO *io, uint64_t value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_UINT64;
    node->uint64_val = value;
}

uint64_t _RedisModule_LoadUnsigned(RedisModuleIO *io) {
    RedisModuleIO *node = RedisRdb_RemoveFirst(io);

    if (!node) {
        fprintf(stderr, "%s:%d: RedisRdb mock EOF\n", __FILE__, __LINE__);
        abort();
    }

    if (node->type != REDIS_MODULE_IO_TYPE_UINT64) {
        fprintf(stderr, "%s:%d: RedisRdb mock node type error\n",
                __FILE__, __LINE__);
        abort();
    }

    const uint64_t value = node->uint64_val;

    free(node);

    return value;
}

void _RedisModule_SaveSigned(RedisModuleIO *io, int64_t value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_INT64;
    node->int64_val = value;
}

int64_t _RedisModule_LoadSigned(RedisModuleIO *io) {
    RedisModuleIO *node = RedisRdb_RemoveFirst(io);

    if (!node) {
        fprintf(stderr, "%s:%d: RedisRdb mock EOF\n", __FILE__, __LINE__);
        abort();
    }

    if (node->type != REDIS_MODULE_IO_TYPE_INT64) {
        fprintf(stderr, "%s:%d: RedisRdb mock node type error\n",
                __FILE__, __LINE__);
        abort();
    }

    const int64_t value = node->int64_val;

    free(node);

    return value;
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
    node->string_size = len;
}

#if 0
RedisModuleString *RedisModule_LoadString(RedisModuleIO *io) {
    /* TODO */
}
#endif

char *_RedisModule_LoadStringBuffer(RedisModuleIO *io, size_t *lenptr) {
    RedisModuleIO *node = RedisRdb_RemoveFirst(io);

    if (!node) {
        fprintf(stderr, "%s:%d: RedisRdb mock EOF\n", __FILE__, __LINE__);
        abort();
    }

    if (node->type != REDIS_MODULE_IO_TYPE_STRING) {
        fprintf(stderr, "%s:%d: RedisRdb mock node type error\n",
                __FILE__, __LINE__);
        abort();
    }

    *lenptr = node->string_size;
    char *str = malloc(node->string_size);

    memcpy(str, node->string, node->string_size);
    free(node);

    return str;
}

void _RedisModule_SaveDouble(RedisModuleIO *io, double value) {
    RedisModuleIO *node = RedisRdb_NewIoNode(io, 0);

    node->type = REDIS_MODULE_IO_TYPE_DOUBLE;
    node->double_val = value;
}

double _RedisModule_LoadDouble(RedisModuleIO *io) {
    RedisModuleIO *node = RedisRdb_RemoveFirst(io);

    if (!node) {
        fprintf(stderr, "RedisRdb mock EOF\n");
        abort();
    }

    const double value = node->double_val;

    free(node);

    return value;
}

static RedisModuleCtx * _RedisModule_GetContextFromIO(RedisModuleIO *io) {
    return &io->ctx;
}

int redis_mock_ctx_flags;
static int _RedisModule_GetContextFlags(RedisModuleCtx *ctx) {
    return redis_mock_ctx_flags;
}

static void _RedisModule_LogIOError(RedisModuleIO *io, const char *level, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "RedisModuleIO error: [%s]: ", level);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
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
RedisModuleCtx * (*RedisModule_GetContextFromIO)(RedisModuleIO *io) = _RedisModule_GetContextFromIO;
int (*RedisModule_GetContextFlags)(RedisModuleCtx *ctx) = _RedisModule_GetContextFlags;
void (*RedisModule_LogIOError)(RedisModuleIO *io, const char *levelstr, const char *fmt, ...) = _RedisModule_LogIOError;
