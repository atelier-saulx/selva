/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include "funmap.h"
#include "selva.h"
#include "hierarchy.h"
#include "selva_lang.h"
#include "selva_object.h"
#include "traversal.h"

int SelvaTraversal_ParseDir2(enum SelvaTraversal *dir, const RedisModuleString *arg) {
    TO_STR(arg);

    if (!strcmp("none", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NONE;
    } else if (!strcmp("node", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_NODE;
    } else if (!strcmp("array", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_ARRAY;
    } else if (!strcmp("set", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_SET;
    } else if (!strcmp("children", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_CHILDREN;
    } else if (!strcmp("parents", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_PARENTS;
    } else if (!strcmp("ancestors", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS;
    } else if (!strcmp("descendants", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS;
    } else if (!strcmp("ref", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_REF;
    } else if (!strcmp("edge_field", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD;
    } else if (!strcmp("bfs_edge_field", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD;
    } else if (!strcmp("bfs_expression", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION;
    } else if (!strcmp("expression", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_EXPRESSION;
    } else if (!strcmp("dfs_full", arg_str)) {
        *dir = SELVA_HIERARCHY_TRAVERSAL_DFS_FULL;
    } else {
        return SELVA_SUBSCRIPTIONS_EINVAL;
    }

    return 0;
}

int SelvaTraversal_FieldsContains(struct SelvaObject *fields, const char *field_name_str, size_t field_name_len) {
    void *iterator;
    const SVector *vec;

    iterator = SelvaObject_ForeachBegin(fields);
    while ((vec = SelvaObject_ForeachValue(fields, &iterator, NULL, SELVA_OBJECT_ARRAY))) {
        struct SVectorIterator it;
        const RedisModuleString *s;

        SVector_ForeachBegin(&it, vec);
        while ((s = SVector_Foreach(&it))) {
            TO_STR(s);

            if (s_len == field_name_len && !strcmp(s_str, field_name_str)) {
                return 1;
            }
        }
    }

    return 0;
}

int SelvaTraversal_GetSkip(enum SelvaTraversal dir) {
    switch (dir) {
     /*
      * Find needs to skip the head node of the traverse for some types as we
      * are not interested in the node we already know.
      */
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION:
        return 1;
    default:
        return 0;
    }
}

const char *SelvaTraversal_Dir2str(enum SelvaTraversal dir) {
    switch (dir) {
    case SELVA_HIERARCHY_TRAVERSAL_NONE:
        return (const char *)"none";
    case SELVA_HIERARCHY_TRAVERSAL_NODE:
        return (const char *)"node";
    case SELVA_HIERARCHY_TRAVERSAL_ARRAY:
        return (const char *)"array";
    case SELVA_HIERARCHY_TRAVERSAL_SET:
        return (const char *)"set";
    case SELVA_HIERARCHY_TRAVERSAL_CHILDREN:
        return (const char *)"children";
    case SELVA_HIERARCHY_TRAVERSAL_PARENTS:
        return (const char *)"parents";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS:
        return (const char *)"bfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS:
        return (const char *)"bfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_ANCESTORS:
        return (const char *)"dfs_ancestors";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_DESCENDANTS:
        return (const char *)"dfs_descendants";
    case SELVA_HIERARCHY_TRAVERSAL_DFS_FULL:
        return (const char *)"dfs_full";
    case SELVA_HIERARCHY_TRAVERSAL_REF:
        return (const char *)"ref";
    case SELVA_HIERARCHY_TRAVERSAL_EDGE_FIELD:
        return (const char *)"edge_field";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EDGE_FIELD:
        return (const char *)"bfs_edge_field";
    case SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION:
        return (const char *)"bfs_expression";
    case SELVA_HIERARCHY_TRAVERSAL_EXPRESSION:
        return (const char *)"expression";
    default:
        return "invalid";
    }
}
