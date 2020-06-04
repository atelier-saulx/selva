#include <punit.h>
#include <stdlib.h>
#include <string.h>
#include "hierarchy.h"
#include "cdefs.h"

SelvaModify_Hierarchy *hierarchy;
Selva_NodeId *findRes;

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
    pu_assert("results poinster was set", findRes != NULL);

    for (size_t i = 0; i < nr_ancestors; i++) {
        pu_assert("the returned ancestor ID is one of the real ancestors", strstr("abc", findRes[i]));
    }

    return NULL;
}

void all_tests(void)
{
    pu_def_test(test_insert_one, PU_RUN);
    pu_def_test(test_insert_many, PU_RUN);
    pu_def_test(test_insert_chain_find_ancestors, PU_RUN);
}
