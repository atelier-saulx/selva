#include <stddef.h>
#include "cdefs.h"
#include "errors.h"
#include "redismodule.h"
#include "alias.h"
#include "selva_set.h"

int SelvaSet_CompareRms(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    RedisModuleString *ra = a->value_rms;
    RedisModuleString *rb = b->value_rms;
    TO_STR(ra, rb);

    if (ra_len < rb_len) {
        return -1;
    }
    if (ra_len > rb_len) {
        return 1;
    }
    return memcmp(ra_str, rb_str, ra_len);
}

int SelvaSet_CompareDouble(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    double da = a->value_d;
    double db = b->value_d;

    return da < db ? -1 : da > db ? 1 : 0;
}

int SelvaSet_CompareLongLong(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    long long lla = a->value_ll;
    long long llb = b->value_ll;

    return lla < llb ? -1 : lla > llb ? 1 : 0;
}

RB_GENERATE(SelvaSetRms, SelvaSetElement, _entry, SelvaSet_CompareRms)
RB_GENERATE(SelvaSetDouble, SelvaSetElement, _entry, SelvaSet_CompareDouble)
RB_GENERATE(SelvaSetLongLong, SelvaSetElement, _entry, SelvaSet_CompareLongLong)

static inline int SelvaSet_Add(struct SelvaSet *set, void *v) {
    enum SelvaSetType type = set->type;

    if (type == SELVA_SET_TYPE_RMSTRING) {
        RedisModuleString *s = (RedisModuleString *)v;
        struct SelvaSetElement *el;

        if (SelvaSet_HasRms(set, s)) {
            return SELVA_EEXIST;
        }

        el = RedisModule_Calloc(1, sizeof(struct SelvaSetElement));
        if (!el) {
            return SELVA_ENOMEM;
        }

        el->value_rms = s;

        (void)RB_INSERT(SelvaSetRms, &set->head_rms, el);
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        double d = *(double *)v;
        struct SelvaSetElement *el;

        if (SelvaSet_HasDouble(set, d)) {
            return SELVA_EEXIST;
        }

        el = RedisModule_Calloc(1, sizeof(struct SelvaSetElement));
        if (!el) {
            return SELVA_ENOMEM;
        }

        el->value_d = d;

        (void)RB_INSERT(SelvaSetDouble, &set->head_d, el);
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        double ll = *(long long *)v;
        struct SelvaSetElement *el;

        if (SelvaSet_HasLongLong(set, ll)) {
            return SELVA_EEXIST;
        }

        el = RedisModule_Calloc(1, sizeof(struct SelvaSetElement));
        if (!el) {
            return SELVA_ENOMEM;
        }

        el->value_ll = ll;

        (void)RB_INSERT(SelvaSetDouble, &set->head_d, el);
    } else {
        return SELVA_EINTYPE;
    }

    set->size++;

    return 0;
}

int SelvaSet_AddRms(struct SelvaSet *set, struct RedisModuleString *s) {
    return SelvaSet_Add(set, s);
}

int SelvaSet_AddDouble(struct SelvaSet *set, double d) {
    return SelvaSet_Add(set, &d);
}

int SelvaSet_AddLongLong(struct SelvaSet *set, long long ll) {
    return SelvaSet_Add(set, &ll);
}

void SelvaSet_DestroyElement(struct SelvaSetElement *el) {
    if (!el) {
        return;
    }

    RedisModule_Free(el);
}

static void SelvaSet_DestroyRms(struct SelvaSet *set) {
    struct SelvaSetRms *head = &set->head_rms;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

	for (el = RB_MIN(SelvaSetRms, head); el != NULL; el = next) {
		next = RB_NEXT(SelvaSetRms, head, el);
		RB_REMOVE(SelvaSetRms, head, el);

        RedisModule_FreeString(NULL, el->value_rms);
        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void SelvaSet_DestroyDouble(struct SelvaSet *set) {
    struct SelvaSetDouble *head = &set->head_d;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

	for (el = RB_MIN(SelvaSetDouble, head); el != NULL; el = next) {
		next = RB_NEXT(SelvaSetDouble, head, el);
		RB_REMOVE(SelvaSetDouble, head, el);

        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

static void SelvaSet_DestroyLongLong(struct SelvaSet *set) {
    struct SelvaSetLongLong *head = &set->head_ll;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

	for (el = RB_MIN(SelvaSetLongLong, head); el != NULL; el = next) {
		next = RB_NEXT(SelvaSetLongLong, head, el);
		RB_REMOVE(SelvaSetLongLong, head, el);

        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

void SelvaSet_Destroy(struct SelvaSet *set) {
    enum SelvaSetType type = set->type;

    if (type == SELVA_SET_TYPE_RMSTRING) {
        SelvaSet_DestroyRms(set);
    } else if (type == SELVA_SET_TYPE_DOUBLE) {
        SelvaSet_DestroyDouble(set);
    } else if (type == SELVA_SET_TYPE_LONGLONG) {
        SelvaSet_DestroyLongLong(set);
    }
}

int SelvaSet_HasRms(struct SelvaSet *set, RedisModuleString *s) {
    struct SelvaSetElement find = {
        .value_rms = s,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_RMSTRING)) {
        return 0;
    }
    fprintf(stderr, "has or has no: %d\n", !!RB_FIND(SelvaSetRms, &set->head_rms, &find));

    return !!RB_FIND(SelvaSetRms, &set->head_rms, &find);
}

int SelvaSet_HasDouble(struct SelvaSet *set, double d) {
    struct SelvaSetElement find = {
        .value_d = d,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_DOUBLE)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetDouble, &set->head_d, &find);
}

int SelvaSet_HasLongLong(struct SelvaSet *set, long long ll) {
    struct SelvaSetElement find = {
        .value_ll = ll,
    };

    if (unlikely(set->type != SELVA_SET_TYPE_LONGLONG)) {
        return 0;
    }

    return !!RB_FIND(SelvaSetLongLong, &set->head_ll, &find);
}

void SelvaSet_RemoveRms(struct SelvaSet *set, RedisModuleString *s) {
    struct SelvaSetElement find = {
        .value_rms = s,
    };

    if (likely(set->type == SELVA_SET_TYPE_RMSTRING)) {
        struct SelvaSetElement *el;

        el = RB_FIND(SelvaSetRms, &set->head_rms, &find);
        if (el && RB_REMOVE(SelvaSetRms, &set->head_rms, el)) {
            set->size--;
        }
    }
}

void SelvaSet_RemoveDouble(struct SelvaSet *set, double d) {
    struct SelvaSetElement find = {
        .value_d = d,
    };

    if (likely(set->type == SELVA_SET_TYPE_DOUBLE)) {
        struct SelvaSetElement *el;

        el = RB_FIND(SelvaSetDouble, &set->head_d, &find);
        if (el && RB_REMOVE(SelvaSetDouble, &set->head_d, el)) {
            set->size--;
        }
    }
}

void SelvaSet_RemoveLongLong(struct SelvaSet *set, long long ll) {
    struct SelvaSetElement find = {
        .value_ll = ll,
    };

    if (likely(set->type == SELVA_SET_TYPE_LONGLONG)) {
        struct SelvaSetElement *el;

        el = RB_FIND(SelvaSetLongLong, &set->head_ll, &find);
        if (el && RB_REMOVE(SelvaSetLongLong, &set->head_ll, el)) {
            set->size--;
        }
    }
}
