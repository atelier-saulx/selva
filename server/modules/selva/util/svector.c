#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "svector.h"

#define VEC_SIZE(_len) (sizeof(void *) * (_len))
#define VEC_COMPAR(_fn) ((int (*)(const void *, const void *))(_fn))

SVector *SVector_Init(SVector *vec, size_t initial_len, int (*compar)(const void **a, const void **b)) {
    *vec = (SVector){
        .vec_compar = compar,
        .vec_len = initial_len < 2 ? 2 : initial_len,
        .vec_last = 0,
        .vec_data = RedisModule_Alloc(VEC_SIZE(initial_len)),
    };

    if (!vec->vec_data) {
        return NULL;
    }

    return vec;
}

void SVector_Destroy(SVector *vec) {
    RedisModule_Free(vec->vec_data);

    vec->vec_len = 0;
    vec->vec_last = 0;
    vec->vec_data = NULL;
}

void SVector_Insert(SVector *vec, void *el) {
    size_t i = vec->vec_last++;
    size_t vec_len = vec->vec_len;
    void **vec_data = vec->vec_data;

    if (i >= vec_len - 1) {
        const size_t new_len = vec_len * 2;
        const size_t new_size = VEC_SIZE(new_len);

        void **new_data = RedisModule_Realloc(vec_data, new_size);
        if (!new_data) {
            new_data = RedisModule_Alloc(new_size);
            if (!new_data) {
                assert(0);
                /* TODO Panic */
                return;
            }

            memcpy(new_data, vec_data, VEC_SIZE(vec->vec_len));
            RedisModule_Free(vec_data);
        }

        vec->vec_data = new_data;
        vec->vec_len = new_len;
        vec_data = new_data;
    }

    vec_data[i] = el;

    if (vec->vec_compar) {
        qsort(vec_data, vec->vec_last, sizeof(void *), VEC_COMPAR(vec->vec_compar));
    }

    assert(vec->vec_last <= vec->vec_len);
}

void *SVector_Search(const SVector * restrict vec, void *key) {
    /* TODO what if vec_compar is not set? */
    assert(("vec_compar must be set", vec->vec_compar));

    void **pp = bsearch(&key, vec->vec_data, vec->vec_last, sizeof(void *), VEC_COMPAR(vec->vec_compar));

    return !pp ? NULL : *pp;
}

void *SVector_Remove(SVector * restrict vec, void *key) {
    /* TODO what if vec_compar is not set? */
    assert(("vec_compar must be set", vec->vec_compar));

    void **pp = bsearch(&key, vec->vec_data, vec->vec_last, sizeof(void *), VEC_COMPAR(vec->vec_compar));
    if (!pp) {
        return NULL;
    }

    void *el = *pp;

    if (vec->vec_last < vec->vec_len) {
        memmove(pp, pp + 1, (size_t)((uintptr_t)(vec->vec_data + vec->vec_last) - (uintptr_t)pp));
    }
    vec->vec_last--;

    assert(vec->vec_last <= vec->vec_len);

    return el;
}

void *SVector_Pop(SVector * restrict vec) {
    if (vec->vec_last > 0) {
        return vec->vec_data[--vec->vec_last];
    }

    assert(vec->vec_last <= vec->vec_len);

    return NULL;
}
