#pragma once
#ifndef REDIS_RDB_H
#define REDIS_RDB_H

enum RedisModuleIOType {
    REDIS_MODULE_IO_TYPE_HEAD = 0,
    REDIS_MODULE_IO_TYPE_UINT64,
    REDIS_MODULE_IO_TYPE_INT64,
    REDIS_MODULE_IO_TYPE_DOUBLE,
    REDIS_MODULE_IO_TYPE_STRING,
};

typedef struct RedisModuleCtx {
    int x;
} RedisModuleCtx;

typedef struct RedisModuleIO {
    enum RedisModuleIOType type;
    struct RedisModuleIO *next;
    size_t string_size;
    struct RedisModuleCtx ctx;
    union {
        struct RedisModuleIO *last;
        uint64_t uint64_val;
        int64_t int64_val;
        double double_val;
        char string[0];
    };
    /* No more fields here, reserved for the data */
} RedisModuleIO;

extern int redis_mock_ctx_flags;
RedisModuleIO *RedisRdb_NewIo(void);
void RedisRdb_FreeIo(RedisModuleIO *io);
size_t RedisRdb_CountIo(RedisModuleIO *io);

/**
 * Print out everything stored in the io mock.
 */
void RedisRdb_Print(RedisModuleIO *io);

#endif /* REDIS_RDB_H */
