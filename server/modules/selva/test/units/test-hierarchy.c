#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"

SelvaModify_Hierarchy *hierarchy;
Selva_NodeId *findRes;

static int SelvaNodeId_Compare(const void *a, const void *b) {

    return strncmp((const char *)a, (const char *)b, SELVA_NODE_ID_SIZE);
}

static void SelvaNodeId_Sort(Selva_NodeId *arr, size_t len) {
    qsort(arr, len, sizeof(Selva_NodeId), SelvaNodeId_Compare);
}

static void setup(void)
{
    hierarchy = SelvaModify_NewHierarchy();
}

static void teardown(void)
{
    SelvaModify_DestroyHierarchy(hierarchy);
    hierarchy = NULL;

    free(findRes);
    findRes = NULL;
}

static char * test_insert_one(void)
{
    const Selva_NodeId id = "abc1";
    const int res = SelvaModify_SetHierarchy(hierarchy, id, 0, NULL, 0, NULL);

    pu_assert_equal("a node was inserted", res, 0);

    return NULL;
}

static char * test_insert_many(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    for (size_t i = 0; i < num_elem(ids); i++) {
        const int res = SelvaModify_SetHierarchy(hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);
    }

    return NULL;
}

static char * test_insert_chain_find_ancestors(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    SelvaModify_SetHierarchy(hierarchy, ids[0], 0, NULL, 0, NULL);
    for (size_t i = 1; i < num_elem(ids); i++) {
        Selva_NodeId parents[1];

        memcpy(parents[0], ids[i - 1], sizeof(Selva_NodeId));
        SelvaModify_SetHierarchy(hierarchy, ids[i], num_elem(parents), parents, 0, NULL);
    }

    SelvaModify_StartHierarchyTrx(hierarchy);
    const int nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "d" }), &findRes);

    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);

    for (size_t i = 0; i < nr_ancestors; i++) {
        pu_assert("the returned ancestor ID is one of the real ancestors", strstr("abc", findRes[i]));
    }

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_1(void)
{
    /*
     *  a -> b --> c
     *   \-->--/
     */

    SelvaModify_SetHierarchy(hierarchy, "a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "b", 1, ((Selva_NodeId []){ "a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "c", 1, ((Selva_NodeId []){ "b", "a" }), 0, NULL);

    SelvaModify_StartHierarchyTrx(hierarchy);
    const int nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "c" }), &findRes);

    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);

    SelvaNodeId_Sort(findRes, nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", findRes[0], "a");
    pu_assert_str_equal("c has a as an ancestor", findRes[1], "b");

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_2(void)
{
    /*
     *  a --> b ---> d --> e
     *   \--->c-->/
     */

    SelvaModify_SetHierarchy(hierarchy, "a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "b", 1, ((Selva_NodeId []){ "a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "c", 1, ((Selva_NodeId []){ "a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "d", 1, ((Selva_NodeId []){ "b", "c" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "e", 1, ((Selva_NodeId []){ "d" }), 0, NULL);

    SelvaModify_StartHierarchyTrx(hierarchy);
    const int nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "c" }), &findRes);

    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);

    pu_assert_str_equal("c has b as an ancestor", findRes[0], "a");

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_3(void)
{
    int nr_ancestors;

    /*
     *  e --> a --> c
     *           /
     *        b --> d
     */

    SelvaModify_SetHierarchy(hierarchy, "a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "c", 2, ((Selva_NodeId []){ "a", "b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "d", 1, ((Selva_NodeId []){ "b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "e", 0, NULL, 1, ((Selva_NodeId []){ "a" }));

    /* ancestors of d */
    //SelvaModify_StartHierarchyTrx(hierarchy);
    //nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "d" }), &findRes);
    //SelvaNodeId_Sort(findRes, nr_ancestors);
    //pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    //pu_assert("results pointer was set", findRes != NULL);
    //pu_assert_str_equal("d has b as an ancestor", findRes[0], "b");

    /* ancestors of c */
    SelvaModify_StartHierarchyTrx(hierarchy);
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "c" }), &findRes);
    SelvaNodeId_Sort(findRes, nr_ancestors);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("c has a as an ancestor", findRes[0], "a");
    pu_assert_str_equal("c has b as an ancestor", findRes[1], "b");
    pu_assert_str_equal("c has e as an ancestor", findRes[2], "e");

    return NULL;
}

void all_tests(void)
{
    //pu_def_test(test_insert_one, PU_RUN);
    //pu_def_test(test_insert_many, PU_RUN);
    //pu_def_test(test_insert_chain_find_ancestors, PU_RUN);
    //pu_def_test(test_insert_acyclic_find_ancestors_1, PU_RUN);
    //pu_def_test(test_insert_acyclic_find_ancestors_2, PU_RUN);
    pu_def_test(test_insert_acyclic_find_ancestors_3, PU_RUN);
}
