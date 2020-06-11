#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "../hierarchy-utils.h"

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

static char * test_add_twice(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    for (size_t i = 0; i < num_elem(ids); i++) {
        int res;

        res = SelvaModify_AddHierarchy(hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);

        res = SelvaModify_AddHierarchy(hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);
    }

    return NULL;
}

static char * test_alter_relationship_set(void)
{
    /*
     *  a ---> c
     *  b ---> d
     * =>
     *  a ---> c
     *  b --/
     *      \> d
     */
    int nr_ancestors;
    int nr_descendants;

    /* Create */
    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* Modify */
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 2, ((Selva_NodeId []){ "grphnode_c", "grphnode_d" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");

    return NULL;
}

static char * test_alter_relationship_add(void)
{
    /*
     *  a ---> c
     *  b ---> d
     * =>
     *  a ---> c
     *  b --/
     *      \> d
     */
    int nr_ancestors;
    int nr_descendants;

    /* Create */
    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* Modify */
    SelvaModify_AddHierarchy(hierarchy, "grphnode_b", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_c" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");

    return NULL;
}

static char * test_get_heads(void)
{
    /*
     *  e --> a --> c
     *           /
     *        b --> d
     */

    int n;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b is a head", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(1), "grphnode_e");

    return NULL;
}

static char * test_get_heads_alter_set(void)
{
    /*
     * b is no longer a head after a new relationship is added
     *
     *  e --> a --> c
     *           /
     *        b --> d
     * =>
     *  e --> a --> c
     *    |     /
     *    \-> b --> d
     */

    int n;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 2, ((Selva_NodeId []){ "grphnode_c", "grphnode_d" }));

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(0), "grphnode_e");

    return NULL;
}

static char * test_get_heads_alter_add(void)
{
    /*
     * b is no longer a head after a new relationship is added
     *
     *  e --> a --> c
     *           /
     *        b --> d
     * =>
     *  e --> a --> c
     *    |     /
     *    \-> b --> d
     */

    int n;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_AddHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 0, NULL);

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(0), "grphnode_e");

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

    const int nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "d" }), &findRes);

    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);

    for (int i = 0; i < nr_ancestors; i++) {
        pu_assert("the returned ancestor ID is one of the real ancestors", strstr("abc", SelvaNodeId_GetRes(i)));
    }

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_1(void)
{
    /*
     *  a -> b --> c
     *   \-->--/
     */

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);

    const int nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);

    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);

    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has a as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_2(void)
{
    /*
     *  a --> b ---> d --> e
     *   \--->c-->/
     */

    int nr_ancestors;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 2,((Selva_NodeId []){ "grphnode_b", "grphnode_c" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 1, ((Selva_NodeId []){ "grphnode_d" }), 0, NULL);

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");

    /* ancestors of e */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 4);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("e has d as an ancestor", SelvaNodeId_GetRes(3), "grphnode_d");
    pu_assert_str_equal("e has c as an ancestor", SelvaNodeId_GetRes(2), "grphnode_c");
    pu_assert_str_equal("e has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("e has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");

    return NULL;
}

static char * test_insert_acyclic_find_ancestors_3(void)
{
    /*
     *  e --> a --> c
     *           /
     *        b --> d
     */

    int nr_ancestors;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("c has e as an ancestor", SelvaNodeId_GetRes(2), "grphnode_e");

    return NULL;
}

static char * test_insert_acyclic_find_descendants_1(void)
{
    /*
     *  a -> b --> c
     *   \-->--/
     */

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_b", "grphnode_a" }), 0, NULL);

    const int nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);

    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has b as a descendant", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");

    return NULL;
}

static char * test_insert_acyclic_find_descendants_2(void)
{
    /*
     *  a --> b ---> d --> e
     *   \--->c-->/
     */

    int nr_descendants;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 2,((Selva_NodeId []){ "grphnode_b", "grphnode_c" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 1, ((Selva_NodeId []){ "grphnode_d" }), 0, NULL);

    /* descendants of c */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(0), "grphnode_d");
    pu_assert_str_equal("b has e as a descendant", SelvaNodeId_GetRes(1), "grphnode_e");

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 4);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has b as a descendant", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    pu_assert_str_equal("a has d as a descendant", SelvaNodeId_GetRes(2), "grphnode_d");
    pu_assert_str_equal("a has e as a descendant", SelvaNodeId_GetRes(3), "grphnode_e");

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(0), "grphnode_d");
    pu_assert_str_equal("b has e as a descendant", SelvaNodeId_GetRes(1), "grphnode_e");

    return NULL;
}

static char * test_insert_acyclic_find_descendants_3(void)
{
    /*
     *  e --> a --> c
     *           /
     *        b --> d
     */

    int nr_descendants;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");

    return NULL;
}

static char * test_insert_acyclic_modify(void)
{
    /*
     *  e --> a --> c
     *           /
     *        b -   d
     */

    int nr_ancestors;
    int nr_descendants;
    int n;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 0, NULL, 0, NULL);

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as a ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as a ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("c has e as a ancestor", SelvaNodeId_GetRes(2), "grphnode_e");

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 0);

    /* heads */
    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(n);
    pu_assert_str_equal("b is a head", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("d is a head", SelvaNodeId_GetRes(1), "grphnode_d");
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(2), "grphnode_e");

    return NULL;
}

static char * test_del_1(void)
{
    /*
     *  e --> a --> c
     *           /
     *        b --> d
     * =>
     *  e --> a --> c
     *           /
     *        b -   d
     */

    int nr_ancestors;
    int nr_descendants;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    SelvaModify_DelHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors for d", nr_ancestors, 0);

    return NULL;
}

static char * test_del_2(void)
{
    /*
     * e is no longer an ancestor of c
     *
     *  e --> a ---> c
     *   \       /
     *    ----b ---> d
     * =>
     *  e --> a   -> c
     *   \       /
     *    ---> b ---> d
     */

    int nr_ancestors;

    SelvaModify_SetHierarchy(hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_AddHierarchy(hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 0, NULL);

    SelvaModify_DelHierarchy(hierarchy, "grphnode_a", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_c" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as a ancestor", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("c has e as a ancestor", SelvaNodeId_GetRes(1), "grphnode_e");

    return NULL;
}


void all_tests(void)
{
    pu_def_test(test_insert_one, PU_RUN);
    pu_def_test(test_insert_many, PU_RUN);
    pu_def_test(test_add_twice, PU_RUN);
    pu_def_test(test_get_heads, PU_RUN);
    pu_def_test(test_get_heads_alter_set, PU_RUN);
    pu_def_test(test_get_heads_alter_add, PU_RUN);
    pu_def_test(test_alter_relationship_set, PU_RUN);
    pu_def_test(test_alter_relationship_add, PU_RUN);
    pu_def_test(test_insert_chain_find_ancestors, PU_RUN);
    pu_def_test(test_insert_acyclic_find_ancestors_1, PU_RUN);
    pu_def_test(test_insert_acyclic_find_ancestors_2, PU_RUN);
    pu_def_test(test_insert_acyclic_find_ancestors_3, PU_RUN);
    pu_def_test(test_insert_acyclic_find_descendants_1, PU_RUN);
    pu_def_test(test_insert_acyclic_find_descendants_2, PU_RUN);
    pu_def_test(test_insert_acyclic_find_descendants_3, PU_RUN);
    pu_def_test(test_insert_acyclic_modify, PU_RUN);
    pu_def_test(test_del_1, PU_RUN);
    pu_def_test(test_del_2, PU_RUN);
}
