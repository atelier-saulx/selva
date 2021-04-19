#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "redismodule.h"
#include "../tunables.h"
#include "mempool.h"
#include "svector.h"

#define VEC_SIZE(_len) (sizeof(void *) * (_len))
#define VEC_COMPAR(_fn) ((int (*)(const void *, const void *))(_fn))

#define SVECTOR_FOREACH_ARR(var, vec) \
    for (typeof(var) var ## _end = (typeof(var))(vec)->vec_arr + (vec)->vec_last, var = (typeof(var))(vec)->vec_arr; \
         (void **)var < (void **)var ## _end; \
         var++)

#define SVECTOR_FOREACH_RBTREE(var, vec) \
        RB_FOREACH((var), SVector_rbtree, (struct SVector_rbtree *)&((vec)->vec_rbhead))

static int svector_rbtree_compar_wrap(struct SVector_rbnode *a, struct SVector_rbnode *b);
RB_PROTOTYPE_STATIC(SVector_rbtree, SVector_rbnode, entry, svector_rbtree_compar_wrap)

static int svector_rbtree_compar_wrap(struct SVector_rbnode *a, struct SVector_rbnode *b) {
    const void * an = a->p;
    const void * bn = b->p;

    assert(a->compar);

    return a->compar(&an, &bn);
}

RB_GENERATE_STATIC(SVector_rbtree, SVector_rbnode, entry, svector_rbtree_compar_wrap)

SVector *SVector_Init(SVector *vec, size_t initial_len, int (*compar)(const void **a, const void **b)) {
    if (unlikely(!vec)) {
        return NULL;
    }

    *vec = (SVector){
        .vec_mode = SVECTOR_MODE_ARRAY,
        .vec_compar = compar,
        .vec_last = 0,
        .vec_arr_len = initial_len,
        .vec_arr_shift_index = 0,
        .vec_arr = NULL,
    };

    if (initial_len > (size_t)0) {
        /* RBTREE mode requires compar function */
        if (initial_len < SVECTOR_THRESHOLD || !compar) {
            vec->vec_arr = RedisModule_Alloc(VEC_SIZE(initial_len));

            if (!vec->vec_arr) {
                return NULL;
            }
        } else {
            vec->vec_mode = SVECTOR_MODE_RBTREE;
            RB_INIT(&vec->vec_rbhead);
            mempool_init(&vec->vec_rbmempool, SVECTOR_SLAB_SIZE, sizeof(struct SVector_rbnode));
        }
    }

    return vec;
}

void SVector_Destroy(SVector *vec) {
    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        RedisModule_Free(vec->vec_arr);
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        mempool_destroy(&vec->vec_rbmempool);
    }

    memset(vec, 0, sizeof(SVector));
}

static void *rbtree_insert(SVector *vec, void *p) {
    struct SVector_rbnode *n;
    struct SVector_rbnode *res;

    assert(p);

    n = mempool_get(&vec->vec_rbmempool);
    if (!n) {
        /*
         * TODO We shouldn't abort here but there is currently no safe way
         * to fail.
         */
        fprintf(stderr, "%s: ENOMEM while allocating from a pool\n", __FILE__);
        abort();
    }

    n->compar = vec->vec_compar;
    n->p = p;
    res = RB_INSERT(SVector_rbtree, &vec->vec_rbhead, n);

    if (res) {
        mempool_return(&vec->vec_rbmempool, n);
        return res->p;
    }

    return NULL;
}

static struct SVector_rbnode *rbtree_find(const SVector * restrict vec, void *key) {
    struct SVector_rbnode n = {
        .compar = vec->vec_compar,
        .p = key,
    };

    return RB_FIND(SVector_rbtree, (struct SVector_rbtree *)&vec->vec_rbhead, &n);
}


static void migrate_arr_to_rbtree(SVector *vec) {
    assert(vec->vec_mode == SVECTOR_MODE_ARRAY);
    assert(vec->vec_compar);

    SVector_ShiftReset(vec);

    const size_t len = SVector_Size(vec);
    const size_t vec_last = vec->vec_last;
    void **vec_arr = vec->vec_arr;

    RB_INIT(&vec->vec_rbhead);
    mempool_init(&vec->vec_rbmempool, SVECTOR_SLAB_SIZE, sizeof(struct SVector_rbnode));

    void **pp;
    for (typeof(pp) pp_end = (typeof(pp))vec_arr + vec_last, pp = (typeof(pp))vec_arr;
         (void **)pp < (void **)pp_end;
         pp++) {
        (void)rbtree_insert(vec, *pp);
    }

    RedisModule_Free(vec_arr);
    vec->vec_mode = SVECTOR_MODE_RBTREE;
    vec->vec_last = len;
    vec->vec_arr_shift_index = 0;
}

SVector *SVector_Clone(SVector *dest, const SVector *src, int (*compar)(const void **a, const void **b)) {
    enum SVectorMode mode = SVector_Mode(src);

    assert(src->vec_arr_shift_index == 0);

    if ((mode != SVECTOR_MODE_ARRAY && mode != SVECTOR_MODE_RBTREE) ||
        !SVector_Init(dest, SVector_Size(src), compar)) {
        return NULL;
    }

    /* Support lazy alloc. */
    if (unlikely(!src->vec_arr)) {
        return dest;
    }

    if (mode == SVECTOR_MODE_ARRAY) {
        void **it;

        SVECTOR_FOREACH_ARR(it, src) {
            void *p = *it;

            /*
             * This shouldn't be required but we do it just in case to be
             * defensive againts anything weird happening.
             */
            if (likely(p)) {
                SVector_Insert(dest, p);
            }
        }
    } else if (mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n;

        RB_FOREACH(n, SVector_rbtree, (struct SVector_rbtree *)&src->vec_rbhead) {
            SVector_Insert(dest, n->p);
        }
    }

    return dest;
}

static size_t calc_new_len(size_t old_len) {
    const size_t new_len = old_len + 1;
    return new_len + (new_len >> 1);
}

static void SVector_Resize(SVector *vec, size_t i) {
    void **vec_arr = vec->vec_arr;
    size_t vec_len = vec->vec_arr_len;

    if (i >= vec_len - 1) {
        const size_t new_len = calc_new_len(vec_len);
        const size_t new_size = VEC_SIZE(new_len);

        void **new_arr = RedisModule_Realloc(vec_arr, new_size);
        if (!new_arr) {
            fprintf(stderr, "SVector realloc failed\n");
            /*
             * TODO We shouldn't abort here but there is absolutely no safe way
             * to fail as of now.
             */
            abort(); /* This will cause a core dump. */
        }

        vec->vec_arr = new_arr;
        vec->vec_arr_len = new_len;
        vec->vec_arr = new_arr;
    }
}

void SVector_Insert(SVector *vec, void *el) {
    if (vec->vec_mode == SVECTOR_MODE_ARRAY &&
        vec->vec_last - vec->vec_arr_shift_index >= SVECTOR_THRESHOLD &&
        vec->vec_compar) {
        migrate_arr_to_rbtree(vec);
    }

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        ssize_t i = vec->vec_last++;
        ssize_t vec_len = vec->vec_arr_len;
        void **vec_arr = vec->vec_arr;

        assert(el);

        SVector_Resize(vec, i);

        vec_arr[i] = el;

        if (vec->vec_compar) {
            qsort(vec_arr + vec->vec_arr_shift_index,
                  vec->vec_last - vec->vec_arr_shift_index,
                  sizeof(void *), VEC_COMPAR(vec->vec_compar));
        }

        assert(vec->vec_last <= vec->vec_arr_len);
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        (void)rbtree_insert(vec, el);
        vec->vec_last++;
    } else {
        abort();
    }
}

void *SVector_InsertFast(SVector *vec, void *el) {
    assert(el);
    assert(vec->vec_compar);

    if (vec->vec_mode == SVECTOR_MODE_ARRAY &&
        vec->vec_last - vec->vec_arr_shift_index >= SVECTOR_THRESHOLD &&
        vec->vec_compar) {
        migrate_arr_to_rbtree(vec);
    }

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        /* Support lazy alloc. */
        if (unlikely(!vec->vec_arr)) {
            const size_t sz = 1;

            vec->vec_arr_len = sz;
            vec->vec_arr = RedisModule_Alloc(VEC_SIZE(sz));
            if (!vec->vec_arr) {
                fprintf(stderr, "SVector realloc failed\n");
                abort(); /* This will cause a core dump. */
            }
        }

        ssize_t l = 0;
        ssize_t r = (ssize_t)vec->vec_last - 1;
        void **vec_arr = vec->vec_arr;

        while (l <= r) {
            ssize_t m = (l + r) / 2;

            assert((ssize_t)m < (ssize_t)vec->vec_last);

            const int rc = vec->vec_compar((const void **)&el, (const void **)vec_arr + m);
            if (rc > 0) {
                l = m + 1;
            } else if (rc < 0) {
                r = m - 1;
            } else {
                /* Already inserted. */
                return vec_arr[m];
            }
        }

        if (vec->vec_last >= vec->vec_arr_len - 1) {
            const size_t new_len = calc_new_len(vec->vec_arr_len);
            const size_t new_size = VEC_SIZE(new_len);

            void **new_arr = RedisModule_Realloc(vec_arr, new_size);
            if (!new_arr) {
                fprintf(stderr, "SVector realloc failed\n");
                abort(); /* This will cause a core dump. */
            }

            vec->vec_arr = new_arr;
            vec->vec_arr_len = new_len;
            vec_arr = new_arr;
        }

        if (l <= (ssize_t)vec->vec_last - 1) {
            memmove(vec_arr + l + 1, vec_arr + l, VEC_SIZE(vec->vec_last - l));
        }
        vec_arr[l] = el;
        vec->vec_last++;

        assert(vec->vec_last <= vec->vec_arr_len);
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        vec->vec_last++;
        return rbtree_insert(vec, el);
    } else {
        abort();
    }

    return NULL;
}

ssize_t SVector_SearchIndex(const SVector * restrict vec, void *key) {
    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        /* The array might be unset in case of lazy alloc was requested. */
        if (unlikely(!vec->vec_arr)) {
            return -1;
        }

        if (vec->vec_compar) {
            void **pp = bsearch(&key, vec->vec_arr + vec->vec_arr_shift_index,
                                vec->vec_last - vec->vec_arr_shift_index,
                                sizeof(void *), VEC_COMPAR(vec->vec_compar));

            if (!pp) {
                return -1;
            }

            return (ptrdiff_t)(pp - vec->vec_arr) - vec->vec_arr_shift_index;
        } else {
            for (size_t i = vec->vec_arr_shift_index; i < vec->vec_last; i++) {
                if (vec->vec_arr[i] == key) {
                    return i;
                }
            }

            return -1;
        }
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n;
        size_t i = 0;

        for (n = RB_MIN(SVector_rbtree, (struct SVector_rbtree *)&vec->vec_rbhead);
             n != NULL;
             n = RB_NEXT(SVector_rbtree, &vec->vec_rbhead, n)) {
            if (vec->vec_compar((const void **)&n->p, (const void **)&key) == 0) {
                return i;
            } else {
                i++;
            }
        }

        return -1;
    } else {
        abort();
    }
}

void *SVector_Search(const SVector * restrict vec, void *key) {
    assert(("vec_compar must be set", vec->vec_compar));

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        /* The array might be unset in case of lazy alloc was requested. */
        if (unlikely(!vec->vec_arr)) {
            return NULL;
        }

        void **pp = bsearch(&key, vec->vec_arr + vec->vec_arr_shift_index,
                            vec->vec_last - vec->vec_arr_shift_index,
                            sizeof(void *), VEC_COMPAR(vec->vec_compar));

        return !pp ? NULL : *pp;
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *res;

        res = rbtree_find(vec, key);

        return !res ? NULL : res->p;
    } else {
        abort();
    }
}

void *SVector_GetIndex(const SVector * restrict vec, size_t index) {
    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        const size_t i = vec->vec_arr_shift_index + index;

        if (i >= vec->vec_last) {
            return NULL;
        }

        return vec->vec_arr[i];
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        size_t i = 0;

        for (struct SVector_rbnode *n = RB_MIN(SVector_rbtree, (struct SVector_rbtree *)&vec->vec_rbhead);
             n != NULL;
             n = RB_NEXT(SVector_rbtree, &vec->vec_rbhead, n)) {
            if (i++ == index) {
                return n;
            }
        }

        return NULL;
    } else {
        abort();
    }
}

void *SVector_RemoveIndex(SVector * restrict vec, size_t index) {
    void *p = NULL;

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        const size_t i = vec->vec_arr_shift_index + index;

        if (i < vec->vec_last) {
            p = vec->vec_arr[i];

            if (vec->vec_last < vec->vec_arr_len) {
                memmove(&vec->vec_arr[i], &vec->vec_arr[i + 1], vec->vec_last - i - 1);
            }
            vec->vec_last--;
        }
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        size_t i = 0;
        struct SVector_rbnode *n;

        for (n = RB_MIN(SVector_rbtree, (struct SVector_rbtree *)&vec->vec_rbhead);
             n != NULL;
             n = RB_NEXT(SVector_rbtree, &vec->vec_rbhead, n)) {
            if (i++ == index) {
                p = n->p;
                RB_REMOVE(SVector_rbtree, &vec->vec_rbhead, n);
                break;
            }
        }
    } else {
        abort();
    }

    return p;
}

// TODO: make sure you null things before the last entry and the first inserted index if it's larger than it's current size
void SVector_InsertIndex(SVector * restrict vec, size_t index, void *el) {
    assert(("vec_compare must not be set", !vec->vec_compar));

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        fprintf(stderr, "THIS IS SPARTA %p %zu %zu\n", vec->vec_arr, vec->vec_last, vec->vec_arr_len);
        SVector_ShiftReset(vec);
        if (index < vec->vec_last) {
            vec->vec_arr[index] = el;
        } else if (index < vec->vec_arr_len) {
            memset(vec->vec_arr + vec->vec_last, 0, vec->vec_arr_len - vec->vec_last);

            vec->vec_arr[index] = el;
            vec->vec_last = index + 1;
        } else {
            SVector_Resize(vec, index);
            SVector_InsertIndex(vec, index, el);
        }
    } else {
        abort();
    }
}

void *SVector_Remove(SVector * restrict vec, void *key) {
    assert(("vec_compar must be set", vec->vec_compar));

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        /* Support lazy alloc. */
        if (unlikely(!vec->vec_arr)) {
            return NULL;
        }

        void **pp = bsearch(&key, vec->vec_arr + vec->vec_arr_shift_index,
                            vec->vec_last - vec->vec_arr_shift_index,
                            sizeof(void *), VEC_COMPAR(vec->vec_compar));
        if (!pp) {
            return NULL;
        }

        void *el = *pp;

        if (vec->vec_last < vec->vec_arr_len) {
            memmove(pp, pp + 1, (size_t)((uintptr_t)(vec->vec_arr + vec->vec_last - 1) - (uintptr_t)pp));
        }
        vec->vec_last--;

        assert(vec->vec_last <= vec->vec_arr_len);

        return el;
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n = rbtree_find(vec, key);
        void *p;

        if (!n) {
            return NULL;
        }

        p = n->p;
        RB_REMOVE(SVector_rbtree, &vec->vec_rbhead, n);
        mempool_return(&vec->vec_rbmempool, n);
        vec->vec_last--;

        return p;
    } else {
        abort();
    }
}

void *SVector_Pop(SVector * restrict vec) {
    void *last = NULL;

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        if (vec->vec_last == vec->vec_arr_shift_index) {
            return NULL;
        }

        assert(vec->vec_last <= vec->vec_arr_len);
        last = vec->vec_arr[--vec->vec_last];
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n = RB_MAX(SVector_rbtree, &vec->vec_rbhead);

        if (!n) {
            return NULL;
        }

        last = n->p;
        RB_REMOVE(SVector_rbtree, &vec->vec_rbhead, last);
        mempool_return(&vec->vec_rbmempool, n);
        vec->vec_last--;
    } else {
        abort();
    }

    return last;
}

void *SVector_Shift(SVector * restrict vec) {
    void *first = NULL;

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        if (vec->vec_last == vec->vec_arr_shift_index) {
            return NULL;
        }
        assert(vec->vec_last <= vec->vec_arr_len);
        assert(vec->vec_arr_shift_index <= vec->vec_last);

        if (vec->vec_arr_shift_index > vec->vec_last / 2) {
            SVector_ShiftReset(vec);
        }

        first = vec->vec_arr[vec->vec_arr_shift_index++];
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n = RB_MIN(SVector_rbtree, &vec->vec_rbhead);

        if (!n) {
            return NULL;
        }

        first = n->p;
        RB_REMOVE(SVector_rbtree, &vec->vec_rbhead, n);
        mempool_return(&vec->vec_rbmempool, n);
        vec->vec_last--;
    } else {
        abort();
    }

    return first;
}

void *SVector_Peek(SVector * restrict vec) {
    void *first = NULL;

    if (vec->vec_mode == SVECTOR_MODE_ARRAY) {
        if (vec->vec_last == vec->vec_arr_shift_index) {
            return NULL;
        }
        assert(vec->vec_last <= vec->vec_arr_len);
        assert(vec->vec_arr_shift_index <= vec->vec_last);

        first = vec->vec_arr[vec->vec_arr_shift_index];
    } else if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbnode *n = RB_MIN(SVector_rbtree, &vec->vec_rbhead);

        if (!n) {
            return NULL;
        }

        first = n->p;
    }

    return first;
}

void SVector_ShiftReset(SVector * restrict vec) {
    if (vec->vec_mode != SVECTOR_MODE_ARRAY || !vec->vec_arr) {
        /* Reseting shift index is only necessary in the array mode. */
        return;
    }

    /*
     * We assume that nobody will call this function when nothing was
     * actually inserted, thus no need to check if vec_arr is NULL.
     */
    vec->vec_last -= vec->vec_arr_shift_index;
    memmove(vec->vec_arr, vec->vec_arr + vec->vec_arr_shift_index, VEC_SIZE(vec->vec_last));
    vec->vec_arr_shift_index = 0;
}

void SVector_Clear(SVector * restrict vec) {
    vec->vec_arr_shift_index = 0;
    vec->vec_last = 0;

    if (vec->vec_mode == SVECTOR_MODE_RBTREE) {
        mempool_destroy(&vec->vec_rbmempool);

        vec->vec_mode = SVECTOR_MODE_ARRAY;
        /* Some defensive programming */
        vec->vec_arr_len = 0;
        vec->vec_arr = NULL;
    }
}

static void *SVector_EmptyForeach(struct SVectorIterator *it __unused) {
    return NULL;
}

static void *SVector_ArrayForeach(struct SVectorIterator *it) {
    if (it->arr.cur < it->arr.end) {
        void **p;

        p = it->arr.cur++;
        return *p;
    }

    return NULL;
}

static void *SVector_RbTreeForeach(struct SVectorIterator *it) {
    struct SVector_rbnode *cur = it->rbtree.next;

    if (!cur) {
        return NULL;
    }

    it->rbtree.next = RB_NEXT(SVector_rbtree, it->rbtree.head, cur);

    return cur->p;
}

void SVector_ForeachBegin(struct SVectorIterator * restrict it, const SVector *vec) {
    assert(it);
    assert(vec);

    it->mode = vec->vec_mode;

    if (it->mode == SVECTOR_MODE_ARRAY) {
        if (!vec->vec_arr) {
            it->fn = SVector_EmptyForeach;
        } else {
            it->arr.cur = vec->vec_arr + vec->vec_arr_shift_index;
            it->arr.end = vec->vec_arr + vec->vec_last;
            it->fn = SVector_ArrayForeach;
        }
    } else if (it->mode == SVECTOR_MODE_RBTREE) {
        struct SVector_rbtree *head = (struct SVector_rbtree *)&vec->vec_rbhead;

        if (RB_EMPTY(head)) {
            it->fn = SVector_EmptyForeach;
        } else {
            it->rbtree.head = head;
            it->rbtree.next = RB_MIN(SVector_rbtree, head);
            it->fn = SVector_RbTreeForeach;
        }
    } else {
        abort();
    }
}
