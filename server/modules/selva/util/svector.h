#pragma once
#ifndef _UTIL_SVECTOR_H_
#define _UTIL_SVECTOR_H_

#include "cdefs.h"

typedef struct SVector {
    int (*vec_compar)(const void **a, const void **b);
    size_t vec_len; /*!< Length of the vector array. */
    size_t vec_shift_index; /*!< Index in the vector array for SVector_Shift(). */
    size_t vec_last; /*!< Length of the vector. (Last index + 1) */
    void **vec_data;
} SVector;

SVector *SVector_Init(SVector *vec, size_t initial_len, int (*compar)(const void **a, const void **b));
void SVector_Destroy(SVector *vec);
SVector *SVector_Clone(SVector *dest, const SVector *src, int (*compar)(const void **a, const void **b));
void SVector_Insert(SVector *vec, void *el);
void *SVector_InsertFast(SVector *vec, void *el);
void *SVector_Search(const SVector * restrict vec, void *key);

/**
 * Get a pointer value from the vector by index.
 */
void *SVector_GetIndex(const SVector * restrict vec, size_t index);
void *SVector_Remove(SVector * restrict vec, void *key);
void *SVector_Pop(SVector * restrict vec);

/**
 * Remove the first element from the vector.
 */
void *SVector_Shift(SVector * restrict vec);
void *SVector_Peek(SVector * restrict vec);

/**
 * Reset the SVector shifting to reduce space required by the vector.
 */
void SVector_ShiftReset(SVector * restrict vec);
void SVector_Clear(SVector * restrict vec);

static inline size_t SVector_Size(const SVector * restrict vec) {
    return vec->vec_last - vec->vec_shift_index;
}

#define SVECTOR_FOREACH(var, vec)                                                                            \
    for (typeof(var) var ## _end = (typeof(var))(vec)->vec_data + (vec)->vec_last, var = (typeof(var))(vec)->vec_data ;  \
         (void **)var < (void **)var ## _end;                                                               \
         var++)

#define svector_autofree __attribute__((cleanup(SVector_Destroy)))

#endif /* _UTIL_SVECTOR_H_ */
