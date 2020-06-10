#pragma once
#ifndef REDIS_RDB_H
#define REDIS_RDB_H

#include <stdint.h>

enum RedisModuleIOType {
    REDIS_MODULE_IO_TYPE_HEAD = 0,
    REDIS_MODULE_IO_TYPE_UINT64,
    REDIS_MODULE_IO_TYPE_INT64,
    REDIS_MODULE_IO_TYPE_DOUBLE,
    REDIS_MODULE_IO_TYPE_STRING,
};

typedef struct RedisModuleIO {
    enum RedisModuleIOType type;
    struct RedisModuleIO *next;
    union {
        struct RedisModuleIO *last;
        uint64_t uint64_val;
        int64_t int64_val;
        double double_val;
        char string[0];
    };
} RedisModuleIO;

RedisModuleIO *RedisRdb_NewIo(void);
void RedisRdb_FreeIo(RedisModuleIO *io);
size_t RedisRdb_CountIo(RedisModuleIO *io);

#endif /* REDIS_RDB_H */
