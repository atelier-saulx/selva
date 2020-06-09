#pragma once
#ifndef _UTIL_SVECTOR_H_
#define _UTIL_SVECTOR_H_

#include "cdefs.h"

typedef struct SVector {
    int (*vec_compar)(const void **a, const void **b);
    size_t vec_len;
    size_t vec_last;
    void **vec_data;
} SVector;

SVector *SVector_Init(SVector *vec, size_t initial_len, int (*compar)(const void **a, const void **b));
void SVector_Destroy(SVector *vec);
void SVector_Insert(SVector *vec, void *el);
void *SVector_Search(const SVector * restrict vec, void *key);
void *SVector_Remove(SVector * restrict vec, void *key);
void *SVector_Pop(SVector * restrict vec);
void SVector_Clear(SVector * restrict vec);

static inline size_t SVector_Size(SVector * restrict vec) {
    return vec->vec_last;
}

#define SVECTOR_FOREACH(var, vec)                                                                            \
    for (typeof(var) var ## _end = (typeof(var))(vec)->vec_data + (vec)->vec_last, var = (typeof(var))(vec)->vec_data ;  \
         (void **)var < (void **)var ## _end;                                                               \
         var++)

#endif /* _UTIL_SVECTOR_H_ */
