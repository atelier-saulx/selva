#include <stdlib.h>

void *RedisModule_Alloc(size_t size) {
    return calloc(1, size);
}

void *RedisModule_Realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void RedisModule_Free(void *ptr) {
    free(ptr);
}
