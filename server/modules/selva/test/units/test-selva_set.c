#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "redismodule.h"
#include "selva.h"
#include "selva_set.h"

static void setup(void)
{
}

static void teardown(void)
{
}

static char * set_add_longlong(void)
{
    struct SelvaSet set;
    struct SelvaSetElement *el;
    size_t n = 0;

    SelvaSet_Init(&set, SELVA_SET_TYPE_LONGLONG);
    pu_assert_equal("added an elem", SelvaSet_Add(&set, 42ll), 0);
    pu_assert_equal("Fails to add the same elem", SelvaSet_Add(&set, 42ll), SELVA_EEXIST);
    pu_assert_equal("set size", SelvaSet_Size(&set), 1);
    pu_assert_equal("has the element", SelvaSet_Has(&set, 42ll), 1);

    SELVA_SET_LONGLONG_FOREACH(el, &set) {
        n++;
        pu_assert_equal("elem value", el->value_ll, 42ll);
        pu_assert_equal("only one elem", n, 1);
    }

    SelvaSet_Destroy(&set);

    return NULL;
}

static char * set_remove_longlong(void)
{
    struct SelvaSet set;
    struct SelvaSetElement *el;

    SelvaSet_Init(&set, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Add(&set, 42ll);
    SelvaSet_Add(&set, 42ll);

    el = SelvaSet_Remove(&set, 42ll);
    pu_assert_not_null("removed an el", el);
    pu_assert_equal("removed the right el", el->value_ll, 42ll);
    SelvaSet_DestroyElement(el);

    el = SelvaSet_Remove(&set, 42ll);
    pu_assert_null("nothing to remove", el);

    pu_assert_equal("set size", SelvaSet_Size(&set), 0);
    pu_assert_equal("has the element", SelvaSet_Has(&set, 42ll), 0);

    SELVA_SET_LONGLONG_FOREACH(el, &set) {
        pu_assert_fail("should not enter the loop");
    }

    SelvaSet_Destroy(&set);

    return NULL;
}

static char * set_merge_longlong(void)
{
    struct SelvaSet setA;
    struct SelvaSet setB;
    struct SelvaSetElement *el;
    int res;

    SelvaSet_Init(&setA, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Init(&setB, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Add(&setB, 1ll);
    SelvaSet_Add(&setB, 2ll);

    pu_assert_equal("merged", SelvaSet_Merge(&setA, &setB), 0);
    pu_assert_equal("set size", SelvaSet_Size(&setA), 2);
    pu_assert_equal("set size", SelvaSet_Size(&setB), 0);

    res = 0;
    SELVA_SET_LONGLONG_FOREACH(el, &setA) {
        res |= 1 << el->value_ll;
    }
    pu_assert_equal("right elems", res, 0x6);

    SELVA_SET_LONGLONG_FOREACH(el, &setB) {
        pu_assert_fail("should not enter the loop");
    }

    SelvaSet_Destroy(&setA);
    SelvaSet_Destroy(&setB);

    return NULL;
}

static char * set_union_longlong(void)
{
    struct SelvaSet setA;
    struct SelvaSet setB;
    struct SelvaSet setC;
    struct SelvaSet setD;
    struct SelvaSetElement *el;
    int res;

    SelvaSet_Init(&setA, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Init(&setB, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Init(&setC, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Init(&setD, SELVA_SET_TYPE_LONGLONG);
    SelvaSet_Add(&setA, 1ll);
    SelvaSet_Add(&setB, 2ll);
    SelvaSet_Add(&setC, 3ll);
    SelvaSet_Add(&setA, 4ll);
    SelvaSet_Add(&setB, 5ll);
    SelvaSet_Add(&setC, 6ll);

    pu_assert_equal("union", SelvaSet_Union(&setD, &setA, &setB, &setC, NULL), 0);
    pu_assert_equal("resulting set size", SelvaSet_Size(&setD), 6);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 1ll), 1);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 2ll), 1);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 3ll), 1);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 4ll), 1);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 5ll), 1);
    pu_assert_equal("resulting set has the right elems", SelvaSet_Has(&setD, 6ll), 1);

    pu_assert_equal("source set size", SelvaSet_Size(&setA), 2);
    pu_assert_equal("source set size", SelvaSet_Size(&setB), 2);
    pu_assert_equal("source set size", SelvaSet_Size(&setC), 2);

    res = 0;
    SELVA_SET_LONGLONG_FOREACH(el, &setD) {
        res |= 1 << el->value_ll;
    }
    pu_assert_equal("foreach found all the elems", res, 0x7e);

    SelvaSet_Destroy(&setA);
    SelvaSet_Destroy(&setB);
    SelvaSet_Destroy(&setC);
    SelvaSet_Destroy(&setD);

    return NULL;
}

void all_tests(void)
{
    pu_def_test(set_add_longlong, PU_RUN);
    pu_def_test(set_remove_longlong, PU_RUN);
    pu_def_test(set_merge_longlong, PU_RUN);
    pu_def_test(set_union_longlong, PU_RUN);
}
