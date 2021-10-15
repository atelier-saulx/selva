#pragma once
#ifndef _UTIL_SVECTOR_H_
#define _UTIL_SVECTOR_H_

#include "cdefs.h"
#include "mempool.h"
#include "tree.h"

enum SVectorMode {
    SVECTOR_MODE_NONE = 0x0,
    SVECTOR_MODE_ARRAY = 0x1,
    SVECTOR_MODE_RBTREE = 0x2,
} __attribute__((packed));

struct SVector;
struct SVectorIterator;

struct SVector_rbnode {
    int (*compar)(const void **a, const void **b);
    RB_ENTRY(SVector_rbnode) entry;
    void *p;
};

RB_HEAD(SVector_rbtree, SVector_rbnode);

struct SVectorIterator {
    enum SVectorMode mode;
    union {
        struct {
            void **cur;
            void **end;
        } arr;
        struct {
            struct SVector_rbtree *head;
            struct SVector_rbnode *next;
        } rbtree;
    };
    void *(*fn)(struct SVectorIterator *it);
};

/* These + 2 should be 64 for optimal packing. */
#define _SVECTOR_ARR_SHIFT_INDEX_BITS 14
#define _SVECTOR_VEC_LAST_BITS 48

/**
 * Allow shifting until the shift_index is filled.
 */
#define _SVECTOR_SHIFT_RESET_THRESHOLD ((1 << _SVECTOR_ARR_SHIFT_INDEX_BITS) - 1)

typedef struct SVector {
    struct {
        uint64_t vec_mode : 2;
        /* Array mode specific. */
        uint64_t vec_arr_shift_index : _SVECTOR_ARR_SHIFT_INDEX_BITS; /*!< Index in the vector array for SVector_Shift(). */
        /* Common to all modes. */
        uint64_t vec_last : _SVECTOR_VEC_LAST_BITS; /*!< Length of the vector. (Last index + 1) */
    };

    union {
        struct {
            /* Array mode specific */
            size_t vec_arr_len; /*!< Length of the vector array. */
            void **vec_arr;
        };
        struct {
            /* RB tree mode specific */
            struct mempool vec_rbmempool;
            struct SVector_rbtree vec_rbhead;
        };
    };

    int (*vec_compar)(const void **a, const void **b);
} SVector;

static inline int SVector_IsInitialized(const SVector *vec) {
    return vec->vec_mode != SVECTOR_MODE_NONE;
}

SVector *SVector_Init(SVector *vec, size_t initial_len, int (*compar)(const void **a, const void **b));
void SVector_Destroy(SVector *vec);
SVector *SVector_Clone(SVector *dest, const SVector *src, int (*compar)(const void **a, const void **b));
void SVector_Insert(SVector *vec, void *el);
void SVector_SetIndex(SVector *vec, size_t idx, void *el);
void *SVector_InsertFast(SVector *vec, void *el);
void SVector_InsertIndex(SVector * restrict vec, size_t index, void *el);

/**
 * Return the index of key in vec.
 * If the SVector is ordered then the vec_compar() function is used for
 * finding the element in the vector; Otherwise the `key` pointer is
 * compared directly to each element in the array to determine its
 * position in the array.
 * @returns Returns the index of key in vec; Otherwise -1.
 */
ssize_t SVector_SearchIndex(const SVector * restrict vec, void *key);
void *SVector_Search(const SVector * restrict vec, void *key);

/**
 * Get a pointer value from the vector by index.
 */
void *SVector_GetIndex(const SVector * restrict vec, size_t index);
void *SVector_RemoveIndex(SVector * restrict vec, size_t index);
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

/**
 * Returns the current length of the vector.
 */
static inline size_t SVector_Size(const SVector * restrict vec) {
    return vec->vec_last - vec->vec_arr_shift_index;
}

static inline enum SVectorMode SVector_Mode(const SVector * restrict vec) {
    return vec->vec_mode;
}

void SVector_ForeachBegin(struct SVectorIterator * restrict it, const SVector *vec);
static inline void *SVector_Foreach(struct SVectorIterator *it) {
    return it->fn(it);
}
int SVector_Done(const struct SVectorIterator *it);

#define SVECTOR_AUTOFREE(name) \
    __attribute__((cleanup(SVector_Destroy))) SVector name = { .vec_mode = SVECTOR_MODE_NONE }

#endif /* _UTIL_SVECTOR_H_ */
