#include <stdlib.h>

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

void * (*RedisModule_Alloc)(size_t size) = _RedisModule_Alloc;
void* (*RedisModule_Calloc)(size_t nmemb, size_t size) = _RedisModule_Calloc;
void * (*RedisModule_Realloc)(void *ptr, size_t size) = _RedisModule_Realloc;
void (*RedisModule_Free)(void *ptr) = _RedisModule_Free;
