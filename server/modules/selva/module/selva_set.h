#pragma once
#ifndef SELVA_SET
#define SELVA_SET

#include "selva.h"
#include "tree.h"

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;

struct SelvaSetElement {
    struct RedisModuleString *value;
    RB_ENTRY(SelvaSetElement) _entry;
};

RB_HEAD(SelvaSetHead, SelvaSetElement);

struct SelvaSet {
    size_t size;
    struct SelvaSetHead head;
};

RB_PROTOTYPE(SelvaSetHead, SelvaSetElement, _entry, SelvaSet_Compare);
void SelvaSet_DestroyElement(struct SelvaSetElement *el);
void SelvaSet_Destroy(struct SelvaSet *head);
struct SelvaSetElement *SelvaSet_Find(struct SelvaSet *set, struct RedisModuleString *v);

static inline void SelvaSet_Init(struct SelvaSet *set) {
    set->size = 0;
    RB_INIT(&set->head);
}

static inline struct SelvaSetElement *SelvaSet_Add(struct SelvaSet *set, struct SelvaSetElement *el) {
    struct SelvaSetElement *old;

    old = RB_INSERT(SelvaSetHead, &set->head, el);
    if (!old) {
        set->size++;
    }

    return old;
}

static inline void SelvaSet_Remove(struct SelvaSet *set, struct SelvaSetElement *el) {
    if (RB_REMOVE(SelvaSetHead, &set->head, el)) {
        set->size--;
    }
}

static inline int SelvaSet_Has(struct SelvaSet *set, RedisModuleString *v) {
    return !!SelvaSet_Find(set, v);
}

#endif /* SELVA_SET */
