#pragma once
#ifndef _UTIL_SVECTOR_H_
#define _UTIL_SVECTOR_H_

#include "cdefs.h"

typedef struct Vector {
    int (*vec_compar)(const void **a, const void **b);
    size_t vec_len;
    size_t vec_last;
    void **vec_data;
} Vector;

Vector *Vector_Init(Vector *vec, size_t initial_len, int (*compar)(const void **a, const void **b));
void Vector_Insert(Vector *vec, void *el);
void *Vector_Search(const Vector * restrict vec, void *key);
void *Vector_Remove(Vector * restrict vec, void *key);

#define VECTOR_FOREACH(var, vec)                                                                            \
    for (typeof(var) var ## _end = (typeof(var))*(vec)->vec_data + (vec)->vec_last, var = *(vec)->vec_data; \
         (void *)var < (void *)var ## _end;                                                                 \
         var++)

#endif /* _UTIL_SVECTOR_H_ */
