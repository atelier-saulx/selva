#pragma once
#ifndef _SELVA_SET_H_
#define _SELVA_SET_H_

#include "selva.h"
#include "tree.h"

struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;

enum SelvaSetType {
    SELVA_SET_TYPE_RMSTRING = 0,
    SELVA_SET_TYPE_DOUBLE = 1,
    SELVA_SET_TYPE_LONGLONG = 2,
    SELVA_SET_TYPE_NODEID = 3,
    SELVA_SET_NR_TYPES,
};

struct SelvaObject;

struct SelvaSetElement {
    union {
        struct RedisModuleString *value_rms;
        double value_d;
        long long value_ll;
        Selva_NodeId value_nodeId;
    };
    RB_ENTRY(SelvaSetElement) _entry;
};

RB_HEAD(SelvaSetRms, SelvaSetElement);
RB_HEAD(SelvaSetDouble, SelvaSetElement);
RB_HEAD(SelvaSetLongLong, SelvaSetElement);
RB_HEAD(SelvaSetNodeId, SelvaSetElement);

struct SelvaSet {
    enum SelvaSetType type;
    unsigned int size;
    union {
        struct SelvaSetRms head_rms;
        struct SelvaSetDouble head_d;
        struct SelvaSetLongLong head_ll;
        struct SelvaSetNodeId head_nodeId;
    };
};

RB_PROTOTYPE(SelvaSetRms, SelvaSetElement, _entry, SelvaSet_CompareRms)
RB_PROTOTYPE(SelvaSetDouble, SelvaSetElement, _entry, SelvaSet_CompareDouble)
RB_PROTOTYPE(SelvaSetLongLong, SelvaSetElement, _entry, SelvaSet_CompareLongLong)
RB_PROTOTYPE(SelvaSetNodeId, SelvaSetElement, _entry, SelvaSet_CompareLongLong)
void SelvaSet_Destroy(struct SelvaSet *head);
void SelvaSet_DestroyElement(struct SelvaSetElement *el);

static inline int SelvaSet_isValidType(enum SelvaSetType type) {
    return type >= 0 && type < SELVA_SET_NR_TYPES;
}

static inline unsigned int SelvaSet_Size(const struct SelvaSet *set) {
    return set->size;
}

static inline void SelvaSet_Init(struct SelvaSet *set, enum SelvaSetType type) {
    set->type = type;
    set->size = 0;

    switch (type) {
    case SELVA_SET_TYPE_RMSTRING:
        RB_INIT(&set->head_rms);
        break;
    case SELVA_SET_TYPE_DOUBLE:
        RB_INIT(&set->head_d);
        break;
    case SELVA_SET_TYPE_LONGLONG:
        RB_INIT(&set->head_ll);
        break;
    case SELVA_SET_TYPE_NODEID:
        RB_INIT(&set->head_nodeId);
        break;
    default:
        /*
         * This should never happen and there is no sane way to recover from an
         * insanely dumb bug, so it's better to abort and fix the bug rather
         * than actually actively checking for this at runtime.
         *
         * We use __builtin_trap() here to avoid requiring a header file for
         * abort(). Typically we use abort() in places where we are pretty
         * certain that there is an ongoing memory corruption and it would be
         * potentially dangerous to run any clean up or RDB save. In this case
         * it's not as certain as usually but if the caller used
         * SelvaSet_isValidType() before calling this function then we can be
         * quite sure that there is something nasty going on somewhere.
         */
        __builtin_trap();
    }
}

int SelvaSet_AddRms(struct SelvaSet *set, struct RedisModuleString *s);
int SelvaSet_AddDouble(struct SelvaSet *set, double d);
int SelvaSet_AddLongLong(struct SelvaSet *set, long long l);
int SelvaSet_AddNodeId(struct SelvaSet *set, const Selva_NodeId node_id);
#define SelvaSet_Add(set, x) _Generic((x), \
        struct RedisModuleString *: SelvaSet_AddRms, \
        double: SelvaSet_AddDouble, \
        long long: SelvaSet_AddLongLong, \
        char *: SelvaSet_AddNodeId, \
        const char *: SelvaSet_AddNodeId \
        )((set), (x))

/**
 * Look for a string equal to s in the SelvaSet set.
 * @returns Returns a pointer to a RedisModuleString stored in set if found;
 *          Otherwise a NULL pointer is returned.
 */
struct RedisModuleString *SelvaSet_FindRms(struct SelvaSet *set, struct RedisModuleString *s);

int SelvaSet_HasRms(struct SelvaSet *set, struct RedisModuleString *s);
int SelvaSet_HasDouble(struct SelvaSet *set, double d);
int SelvaSet_HasLongLong(struct SelvaSet *set, long long ll);
int SelvaSet_HasNodeId(struct SelvaSet *set, const Selva_NodeId node_id);
#define SelvaSet_Has(set, x) _Generic((x), \
        struct RedisModuleString *: SelvaSet_HasRms, \
        double: SelvaSet_HasDouble, \
        long long: SelvaSet_HasLongLong, \
        char *: SelvaSet_HasNodeId, \
        const char *: SelvaSet_HasNodeId \
        )((set), (x))

struct SelvaSetElement *SelvaSet_RemoveRms(struct SelvaSet *set, struct RedisModuleString *s);
struct SelvaSetElement *SelvaSet_RemoveDouble(struct SelvaSet *set, double d);
struct SelvaSetElement *SelvaSet_RemoveLongLong(struct SelvaSet *set, long long ll);
struct SelvaSetElement *SelvaSet_RemoveNodeId(struct SelvaSet *set, const Selva_NodeId node_id);
#define SelvaSet_Remove(set, x) _Generic((x), \
        struct RedisModuleString *: SelvaSet_RemoveRms, \
        double: SelvaSet_RemoveDouble, \
        long long: SelvaSet_RemoveLongLong, \
        char *: SelvaSet_RemoveNodeId, \
        const char *: SelvaSet_RemoveNodeId \
        )((set), (x))

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

#define SELVA_SET_NODEID_FOREACH(el, set) \
    RB_FOREACH(el, SelvaSetNodeId, &(set)->head_nodeId)

#define SELVA_SET_NODEID_FOREACH_SAFE(el, set, tmp) \
    RB_FOREACH_SAFE(el, SelvaSetNodeId, &(set)->head_nodeId, tmp)

/**
 * Move elements from src to dst.
 * Only elements that are currently missing from dst are moved.
 * @param res should be an empty set initialized with the right type.
 */
int SelvaSet_Merge(struct SelvaSet *dst, struct SelvaSet *src);

/**
 * Take an union of the given sets.
 * The elements are cloned and RedisModuleStrings are refcounted by calling
 * RedisModule_HoldString().
 * The last argument of this function must be NULL.
 * @param res should be an empty set initialized with the right type.
 *            `res` must not be a pointer to one of the source sets.
 */
int SelvaSet_Union(struct SelvaSet *res, ...);

#endif /* _SELVA_SET_H_ */
