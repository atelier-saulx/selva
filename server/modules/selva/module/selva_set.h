#pragma once
#ifndef SELVA_SET
#define SELVA_SET

#include "selva.h"
#include "tree.h"

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;

enum SelvaSetType {
    SELVA_SET_TYPE_RMSTRING = 0,
    SELVA_SET_TYPE_DOUBLE = 1,
    SELVA_SET_TYPE_LONGLONG = 2,
};

struct SelvaObject;

struct SelvaSetElement {
    union {
        struct RedisModuleString *value_rms;
        double value_d;
        long long value_ll;
    };
    RB_ENTRY(SelvaSetElement) _entry;
};

RB_HEAD(SelvaSetRms, SelvaSetElement);
RB_HEAD(SelvaSetDouble, SelvaSetElement);
RB_HEAD(SelvaSetLongLong, SelvaSetElement);

struct SelvaSet {
    enum SelvaSetType type;
    size_t size;
    union {
        struct SelvaSetRms head_rms;
        struct SelvaSetDouble head_d;
        struct SelvaSetLongLong head_ll;
    };
};

RB_PROTOTYPE(SelvaSetRms, SelvaSetElement, _entry, SelvaSet_CompareRms)
RB_PROTOTYPE(SelvaSetDouble, SelvaSetElement, _entry, SelvaSet_CompareDouble)
RB_PROTOTYPE(SelvaSetLongLong, SelvaSetElement, _entry, SelvaSet_CompareLongLong)
void SelvaSet_Destroy(struct SelvaSet *head);
void SelvaSet_DestroyElement(struct SelvaSetElement *el);

static inline void SelvaSet_Init(struct SelvaSet *set, enum SelvaSetType type) {
    set->type = type;
    set->size = 0;

    if (type == SELVA_SET_TYPE_RMSTRING) {
        RB_INIT(&set->head_rms);
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        RB_INIT(&set->head_d);
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        RB_INIT(&set->head_ll);
    } else {
        /* TODO What to do if type is invalid */
    }
}

int SelvaSet_AddRms(struct SelvaSet *set, struct RedisModuleString *s);
int SelvaSet_AddDouble(struct SelvaSet *set, double d);
int SelvaSet_AddLongLong(struct SelvaSet *set, long long l);
int SelvaSet_HasRms(struct SelvaSet *set, RedisModuleString *s);
int SelvaSet_HasDouble(struct SelvaSet *set, double d);
int SelvaSet_HasLongLong(struct SelvaSet *set, long long ll);
struct SelvaSetElement *SelvaSet_RemoveRms(struct SelvaSet *set, RedisModuleString *s);
struct SelvaSetElement *SelvaSet_RemoveDouble(struct SelvaSet *set, double d);
struct SelvaSetElement *SelvaSet_RemoveLongLong(struct SelvaSet *set, long long ll);

#define SELVA_SET_RMS_FOREACH(el, set) \
    RB_FOREACH(el, SelvaSetRms, &(set)->head_rms)

#define SELVA_SET_RMS_FOREACH_SAFE(el, set, tmp) \
    RB_FOREACH_SAFE(el, SelvaSetRms, &(set)->head_rms, tmp)

#define SELVA_SET_DOUBLE_FOREACH(el, set) \
    RB_FOREACH(el, SelvaSetDouble, &(set)->head_d)

#define SELVA_SET_DOUBLE_FOREACH_SAFE(el, set, tmp) \
    RB_FOREACH_SAFE(el, SelvaSetDouble, &(set)->head_d, tmp)

#define SELVA_SET_LONGLONG_FOREACH(el, set) \
    RB_FOREACH(el, SelvaSetLongLong, &(set)->head_ll)

#define SELVA_SET_LONGLONG_FOREACH_SAFE(el, set, tmp) \
    RB_FOREACH_SAFE(el, SelvaSetLongLong, &(set)->head_ll, tmp)

#endif /* SELVA_SET */
