#include <punit.h>
#include <stdlib.h>
#include "hierarchy.h"
#include "edge.h"
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

static char * test_get_constraint(void)
{
    const struct EdgeFieldConstraint *constraint;

    constraint = Edge_GetConstraint(0);
    pu_assert("got the default constraint", Edge_GetConstraint(0));
    pu_assert("got null", !Edge_GetConstraint((unsigned)(-1)));

    return NULL;
}

static char * test_alter_edge_relationship(void)
{
    /*
     *  a.a ---> c
     *  b.a ---> d
     * =>
     *  a.a ----> c
     *  b.a --/
     *        \> d
     */

    /* Create nodes. */
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_d", 0, NULL, 0, NULL);

    /* Add edges. */
    pu_assert("Add edge", !Edge_Add("a", 1, 0, SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")));
    pu_assert("Add edge", !Edge_Add("a", 1, 0, SelvaHierarchy_FindNode(hierarchy, "grphnode_b"), SelvaHierarchy_FindNode(hierarchy, "grphnode_d")));

    pu_assert_equal("a.a has c", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")), 1);
    pu_assert_equal("b.a has d", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_b"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_d")), 1);

    /* Alter edges. */
    pu_assert("Add edge", !Edge_Add("a", 1, 0, SelvaHierarchy_FindNode(hierarchy, "grphnode_b"), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")));

    pu_assert_equal("a.a has c", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")), 1);
    pu_assert_equal("b.a has c", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_b"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")), 1);
    pu_assert_equal("b.a has d", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_b"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_d")), 1);

    return NULL;
}

static char * test_delete_edge(void)
{
    /* Create nodes. */
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_a", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_b", 0, NULL, 0, NULL);
    SelvaModify_SetHierarchy(NULL, hierarchy, "grphnode_c", 0, NULL, 0, NULL);

    /* Add edges. */
    pu_assert("Add edge", !Edge_Add("a", 1, 0, SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), SelvaHierarchy_FindNode(hierarchy, "grphnode_b")));
    pu_assert("Add edge", !Edge_Add("a", 1, 0, SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")));

    /* Delete an edge. */
    struct SelvaModify_HierarchyNode *node_a = SelvaHierarchy_FindNode(hierarchy, "grphnode_a");
    Edge_Delete(Edge_GetField(node_a, "a", 1), node_a, SelvaHierarchy_FindNode(hierarchy, "grphnode_b"));

    pu_assert_equal("a.a doesn't have b", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_b")), 0);
    pu_assert_equal("b.a has c", Edge_Has(Edge_GetField(SelvaHierarchy_FindNode(hierarchy, "grphnode_a"), "a", 1), SelvaHierarchy_FindNode(hierarchy, "grphnode_c")), 1);

    return NULL;
}

static char * test_delete_edge_field(void)
{

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_get_constraint, PU_RUN);
    pu_def_test(test_alter_edge_relationship, PU_RUN);
    pu_def_test(test_delete_edge, PU_RUN);
}
