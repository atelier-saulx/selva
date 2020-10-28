#include <stddef.h>
#include "cdefs.h"
#include "errors.h"
#include "redismodule.h"
#include "alias.h"
#include "selva_set.h"

int SelvaSet_Compare(struct SelvaSetElement *a, struct SelvaSetElement *b) {
    RedisModuleString *ra = a->value;
    RedisModuleString *rb = b->value;
    TO_STR(ra, rb);

    if (ra_len < rb_len) {
        return -1;
    }
    if (ra_len > rb_len) {
        return 1;
    }
    return memcmp(ra_str, rb_str, ra_len);
}

RB_GENERATE(SelvaSetHead, SelvaSetElement, _entry, SelvaSet_Compare);

void SelvaSet_DestroyElement(struct SelvaSetElement *el) {
    if (!el) {
        return;
    }

    RedisModule_FreeString(NULL, el->value);
    RedisModule_Free(el);
}

void SelvaSet_Destroy(struct SelvaSet *set) {
    struct SelvaSetHead *head = &set->head;
    struct SelvaSetElement *el;
    struct SelvaSetElement *next;

	for (el = RB_MIN(SelvaSetHead, head); el != NULL; el = next) {
		next = RB_NEXT(SelvaSetHead, head, el);
		RB_REMOVE(SelvaSetHead, head, el);
        SelvaSet_DestroyElement(el);
    }
    set->size = 0;
}

struct SelvaSetElement *SelvaSet_Find(struct SelvaSet *set, struct RedisModuleString *v) {
    struct SelvaSetElement find = {
        .value = v,
    };

    return RB_FIND(SelvaSetHead, &set->head, &find);
}
