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
        .vec_shift_index = 0,
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

SVector *SVector_Clone(SVector *dest, const SVector *src, int (*compar)(const void **a, const void **b)) {
    void **it;

    if (!SVector_Init(dest, SVector_Size(src), compar)) {
        return NULL;
    }

    SVECTOR_FOREACH(it, src) {
        SVector_Insert(dest, *it);
    }

    return dest;
}

void SVector_Insert(SVector *vec, void *el) {
    size_t i = vec->vec_last++;
    size_t vec_len = vec->vec_len;
    void **vec_data = vec->vec_data;

    assert(el);

    if (i >= vec_len - 1) {
        const size_t new_len = vec_len * 2;
        const size_t new_size = VEC_SIZE(new_len);

        void **new_data = RedisModule_Realloc(vec_data, new_size);
        if (!new_data) {
            fprintf(stderr, "SVector realloc failed\n");
            abort(); /* This will cause a core dump. */
        }

        vec->vec_data = new_data;
        vec->vec_len = new_len;
        vec_data = new_data;
    }

    vec_data[i] = el;

    if (vec->vec_compar) {
        qsort(vec_data + vec->vec_shift_index, vec->vec_last,
              sizeof(void *), VEC_COMPAR(vec->vec_compar));
    }

    assert(vec->vec_last <= vec->vec_len);
}

void *SVector_InsertFast(SVector *vec, void *el) {
    ssize_t l = 0;
    ssize_t r = (ssize_t)vec->vec_last - 1;
    void **vec_data = vec->vec_data;

    assert(el);
    assert(vec->vec_compar);

    while (l <= r) {
        ssize_t m = (l + r) / 2;

        const int rc = vec->vec_compar((const void **)&el, (const void **)vec_data + m);
        if (rc > 0) {
            l = m + 1;
        } else if (rc < 0) {
            r = m - 1;
        } else {
            /* Already inserted. */
            return vec_data[m];
        }
    }

    if (vec->vec_last >= vec->vec_len - 1) {
        const size_t new_len = vec->vec_len * 2;
        const size_t new_size = VEC_SIZE(new_len);

        void **new_data = RedisModule_Realloc(vec_data, new_size);
        if (!new_data) {
            fprintf(stderr, "SVector realloc failed\n");
            abort(); /* This will cause a core dump. */
        }

        vec->vec_data = new_data;
        vec->vec_len = new_len;
        vec_data = new_data;
    }

    if (l <= (ssize_t)vec->vec_last - 1) {
        memmove(vec_data + l + 1, vec_data + l, (vec->vec_last - l) * sizeof(void *));
    }
    vec_data[l] = el;
    vec->vec_last++;

    assert(vec->vec_last <= vec->vec_len);

    return NULL;
}

void *SVector_Search(const SVector * restrict vec, void *key) {
    assert(("vec_compar must be set", vec->vec_compar));

    void **pp = bsearch(&key, vec->vec_data + vec->vec_shift_index,
                        vec->vec_last, sizeof(void *), VEC_COMPAR(vec->vec_compar));

    return !pp ? NULL : *pp;
}

void *SVector_Remove(SVector * restrict vec, void *key) {
    assert(("vec_compar must be set", vec->vec_compar));

    void **pp = bsearch(&key, vec->vec_data + vec->vec_shift_index,
                        vec->vec_last, sizeof(void *), VEC_COMPAR(vec->vec_compar));
    if (!pp) {
        return NULL;
    }

    void *el = *pp;

    if (vec->vec_last < vec->vec_len) {
        memmove(pp, pp + 1, (size_t)((uintptr_t)(vec->vec_data + vec->vec_last - 1) - (uintptr_t)pp));
    }
    vec->vec_last--;

    assert(vec->vec_last <= vec->vec_len);

    return el;
}

void *SVector_Pop(SVector * restrict vec) {
    if (vec->vec_last == vec->vec_shift_index) {
        return NULL;
    }

    assert(vec->vec_last <= vec->vec_len);
    return vec->vec_data[--vec->vec_last];
}

void *SVector_Shift(SVector * restrict vec) {
    void *first;

    if (vec->vec_last == vec->vec_shift_index) {
        return NULL;
    }
    assert(vec->vec_last <= vec->vec_len);
    assert(vec->vec_shift_index <= vec->vec_last);

    if (vec->vec_shift_index > vec->vec_last / 2) {
        SVector_ShiftReset(vec);
    }

    first = vec->vec_data[vec->vec_shift_index++];

    return first;
}

void *SVector_Peek(SVector * restrict vec) {
    void *first;

    if (vec->vec_last == vec->vec_shift_index) {
        return NULL;
    }
    assert(vec->vec_last <= vec->vec_len);
    assert(vec->vec_shift_index <= vec->vec_last);

    first = vec->vec_data[vec->vec_shift_index];

    return first;
}

void SVector_ShiftReset(SVector * restrict vec) {
    memmove(vec->vec_data, vec->vec_data + vec->vec_shift_index, VEC_SIZE(vec->vec_last));
    vec->vec_last -= vec->vec_shift_index;
    vec->vec_shift_index = 0;
}

void SVector_Clear(SVector * restrict vec) {
    vec->vec_shift_index = 0;
    vec->vec_last = 0;
}
