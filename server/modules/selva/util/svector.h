#pragma once
#ifndef _UTIL_SVECTOR_H_
#define _UTIL_SVECTOR_H_

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

#endif /* _UTIL_SVECTOR_H_ */
