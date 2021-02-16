#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"
#include "errors.h"
#include "../hierarchy-utils.h"

static void setup(void)
{
    hierarchy = SelvaModify_NewHierarchy(NULL);
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
    const int res = SelvaModify_SetHierarchy(NULL, hierarchy, id, 0, NULL, 0, NULL);

    pu_assert_equal("a node was inserted", res, 0);

    return NULL;
}

static char * test_insert_many(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    for (size_t i = 0; i < num_elem(ids); i++) {
        const int res = SelvaModify_SetHierarchy(NULL, hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);
    }

    return NULL;
}

static char * test_set_nonexisting_parent(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_SetHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_SetHierarchy(NULL, hierarchy, ids[2], 1, &ids[1], 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    return NULL;
}

static char * test_set_nonexisting_child(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_SetHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_SetHierarchy(NULL, hierarchy, ids[1], 0, NULL, 1, &ids[2]);
    pu_assert_equal("a node was inserted", err, 0);

    return NULL;
}

static char * test_add_twice(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    for (size_t i = 0; i < num_elem(ids); i++) {
        int res;

        res = SelvaModify_AddHierarchy(NULL, hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);

        res = SelvaModify_AddHierarchy(NULL, hierarchy, ids[i], 0, NULL, 0, NULL);
        pu_assert_equal("a node was inserted", res, 0);
    }

    return NULL;
}

static char * test_add_nonexisting_parent_1(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[2], 1, &ids[1], 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    return NULL;
}

static char * test_add_nonexisting_parent_2(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 1, &ids[1], 0, NULL);
    pu_assert_equal("no error even if the parent didn't exist", err, 0);

    return NULL;
}

static char * test_add_nonexisting_child_1(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[1], 0, NULL, 1, &ids[2]);
    pu_assert_equal("a node was inserted", err, 0);

    return NULL;
}

static char * test_add_nonexisting_child_2(void)
{
    int err;
    const Selva_NodeId ids[] = { "grphnode_a", "grphnode_b", "grphnode_c" };

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    pu_assert_equal("a node was inserted", err, 0);

    err = SelvaModify_AddHierarchy(NULL, hierarchy, ids[0], 0, NULL, 1, &ids[2]);
    pu_assert_equal("no error even if the child didn't exist", err, 0);

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
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* Modify */
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 2, ((Selva_NodeId []){ "grphnode_c", "grphnode_d" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");
    free(findRes);
    findRes = NULL;

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
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* Modify */
    SelvaModify_AddHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_c" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");
    free(findRes);
    findRes = NULL;

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 3);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b is a head", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(1), "grphnode_e");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(2), "root");
    free(findRes);
    findRes = NULL;

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 2, ((Selva_NodeId []){ "grphnode_c", "grphnode_d" }));

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(0), "grphnode_e");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(1), "root");
    free(findRes);
    findRes = NULL;

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_AddHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 0, NULL);

    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(0), "grphnode_e");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(1), "root");
    free(findRes);
    findRes = NULL;

    return NULL;
}

static char * test_insert_chain_find_ancestors(void)
{
    const Selva_NodeId ids[] = { "a", "b", "c", "d", "e", "f" };

    SelvaModify_SetHierarchy(NULL, hierarchy, ids[0], 0, NULL, 0, NULL);
    for (size_t i = 1; i < num_elem(ids); i++) {
        Selva_NodeId parents[1];

        memcpy(parents[0], ids[i - 1], sizeof(Selva_NodeId));
        SelvaModify_SetHierarchy(NULL, hierarchy, ids[i], num_elem(parents), parents, 0, NULL);
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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 2,((Selva_NodeId []){ "grphnode_b", "grphnode_c" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 1, ((Selva_NodeId []){ "grphnode_d" }), 0, NULL);

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    free(findRes);
    findRes = NULL;

    /* ancestors of e */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 4);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("e has d as an ancestor", SelvaNodeId_GetRes(3), "grphnode_d");
    pu_assert_str_equal("e has c as an ancestor", SelvaNodeId_GetRes(2), "grphnode_c");
    pu_assert_str_equal("e has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("e has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    free(findRes);
    findRes = NULL;

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 1);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("d has b as an ancestor", SelvaNodeId_GetRes(0), "grphnode_b");
    free(findRes);
    findRes = NULL;

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as an ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as an ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("c has e as an ancestor", SelvaNodeId_GetRes(2), "grphnode_e");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 1);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    return NULL;
}

static char * test_insert_acyclic_find_descendants_1(void)
{
    /*
     *  a -> b --> c
     *   \-->--/
     */

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_b", "grphnode_a" }), 0, NULL);

    const int nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has b as a descendant", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 0);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 1);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
#endif

    return NULL;
}

static char * test_insert_acyclic_find_descendants_2(void)
{
    /*
     *  a --> b ---> d --> e
     *   \--->c-->/
     */

    int nr_descendants;

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 1, ((Selva_NodeId []){ "grphnode_a" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 2,((Selva_NodeId []){ "grphnode_b", "grphnode_c" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 1, ((Selva_NodeId []){ "grphnode_d" }), 0, NULL);

    /* descendants of c */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(0), "grphnode_d");
    pu_assert_str_equal("b has e as a descendant", SelvaNodeId_GetRes(1), "grphnode_e");
    free(findRes);
    findRes = NULL;

    /* descendants of a */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_a" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 4);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("a has b as a descendant", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("a has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    pu_assert_str_equal("a has d as a descendant", SelvaNodeId_GetRes(2), "grphnode_d");
    pu_assert_str_equal("a has e as a descendant", SelvaNodeId_GetRes(3), "grphnode_e");
    free(findRes);
    findRes = NULL;

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(0), "grphnode_d");
    pu_assert_str_equal("b has e as a descendant", SelvaNodeId_GetRes(1), "grphnode_e");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 0);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 1);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 1);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 2);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 3);
#endif

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    pu_assert_str_equal("b has d as a descendant", SelvaNodeId_GetRes(1), "grphnode_d");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 1);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 0, NULL, 0, NULL);

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* descendants of e */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_descendants);
    pu_assert_str_equal("e has a as a descendant", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("e has c as a descendant", SelvaNodeId_GetRes(1), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 3);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has a as a ancestor", SelvaNodeId_GetRes(0), "grphnode_a");
    pu_assert_str_equal("c has b as a ancestor", SelvaNodeId_GetRes(1), "grphnode_b");
    pu_assert_str_equal("c has e as a ancestor", SelvaNodeId_GetRes(2), "grphnode_e");
    free(findRes);
    findRes = NULL;

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 0);
    free(findRes);
    findRes = NULL;

    /* heads */
    n = SelvaModify_GetHierarchyHeads(hierarchy, &findRes);
    pu_assert_equal("returned the right number of heads", n, 4);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(n);
    pu_assert_str_equal("b is a head", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("d is a head", SelvaNodeId_GetRes(1), "grphnode_d");
    pu_assert_str_equal("e is a head", SelvaNodeId_GetRes(2), "grphnode_e");
    pu_assert_str_equal("root is a head", SelvaNodeId_GetRes(3), "root");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 0);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 1);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    SelvaModify_DelHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);

    /* descendants of b */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_b" }), &findRes);
    pu_assert_equal("returned the right number of descendants", nr_descendants, 1);
    pu_assert("results pointer was set", findRes != NULL);
    pu_assert_str_equal("b has c as a descendant", SelvaNodeId_GetRes(0), "grphnode_c");
    free(findRes);
    findRes = NULL;

    /* ancestors of d */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_d" }), &findRes);
    pu_assert_equal("returned the right number of ancestors for d", nr_ancestors, 0);
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 0);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

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

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_AddHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 0, NULL);

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 1);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 2);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    SelvaModify_DelHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_c" }));

    /* ancestors of c */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "grphnode_c" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 2);
    pu_assert("results pointer was set", findRes != NULL);
    SelvaNodeId_SortRes(nr_ancestors);
    pu_assert_str_equal("c has b as a ancestor", SelvaNodeId_GetRes(0), "grphnode_b");
    pu_assert_str_equal("c has e as a ancestor", SelvaNodeId_GetRes(1), "grphnode_e");
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 1);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 2);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    return NULL;
}

static char * test_del_node(void)
{
    /*
     * e is removed completely
     *
     *  e --> a ---> c
     *   \       /
     *    ----b ---> d
     * =>
     *        a ----> c
     *           /
     *         b ---> d
     */

    int nr_ancestors;
    int nr_descendants;

    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 2, ((Selva_NodeId []){ "grphnode_a", "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 1, ((Selva_NodeId []){ "grphnode_b" }), 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_e", 0, NULL, 1, ((Selva_NodeId []){ "grphnode_a" }));
    SelvaModify_AddHierarchy(NULL, hierarchy, "grphnode_b", 1, ((Selva_NodeId []){ "grphnode_e" }), 0, NULL);

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 1);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 1);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 2);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 2);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    SelvaModify_DelHierarchyNode(NULL, hierarchy, ((Selva_NodeId){ "grphnode_e" }));

    /* ancestors of root */
    nr_ancestors = SelvaModify_FindAncestors(hierarchy, ((Selva_NodeId){ "root" }), &findRes);
    pu_assert_equal("returned the right number of ancestors", nr_ancestors, 0);
    pu_assert("results pointer was not set", findRes == NULL);
    free(findRes);
    findRes = NULL;

    /* e doesn't exist */
    nr_descendants = SelvaModify_FindDescendants(hierarchy, ((Selva_NodeId){ "grphnode_e" }), &findRes);
    pu_assert_equal("nothing found", nr_descendants, SELVA_MODIFY_HIERARCHY_ENOENT);
    free(findRes);
    findRes = NULL;

#if HIERARCHY_SORT_BY_DEPTH
    /*
     * Depth.
     */
    pu_assert_equal("depth of a is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_a"), 0);
    pu_assert_equal("depth of b is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_b"), 0);
    pu_assert_equal("depth of c is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_c"), 0);
    pu_assert_equal("depth of d is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_d"), 0);
    pu_assert_equal("depth of e is", SelvaModify_GetHierarchyDepth(hierarchy, "grphnode_e"), 0);
#endif

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_insert_one, PU_RUN);
    pu_def_test(test_insert_many, PU_RUN);
    pu_def_test(test_set_nonexisting_parent, PU_RUN);
    pu_def_test(test_set_nonexisting_child, PU_RUN);
    pu_def_test(test_add_twice, PU_RUN);
    pu_def_test(test_get_heads, PU_RUN);
    pu_def_test(test_get_heads_alter_set, PU_RUN);
    pu_def_test(test_get_heads_alter_add, PU_RUN);
    pu_def_test(test_add_nonexisting_parent_1, PU_RUN);
    pu_def_test(test_add_nonexisting_parent_2, PU_RUN);
    pu_def_test(test_add_nonexisting_child_1, PU_RUN);
    pu_def_test(test_add_nonexisting_child_2, PU_RUN);
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
    pu_def_test(test_del_node, PU_RUN);
}
