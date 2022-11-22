/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "util/selva_string.h"
#include "selva_db.h"
#include "comparator.h"

int SelvaSVectorComparator_Cstring(const void ** restrict ap, const void ** restrict bp) {
    const char *a = *(const char **)ap;
    const char *b = *(const char **)bp;

    return strcmp(a, b);
}

int SelvaSVectorComparator_NodeId(const void ** restrict ap, const void ** restrict bp) {
    const Selva_NodeId *a = *(const Selva_NodeId **)ap;
    const Selva_NodeId *b = *(const Selva_NodeId **)bp;

    return memcmp(a, b, SELVA_NODE_ID_SIZE);
}

int SelvaSVectorComparator_String(const void ** restrict ap, const void ** restrict bp) {
    const struct selva_string *a = *(const struct selva_string **)ap;
    const struct selva_string *b = *(const struct selva_string **)bp;

    return selva_string_cmp(a, b);
}

int SelvaSVectorComparator_Node(const void ** restrict a, const void ** restrict b) {
    /* We expect that the node struct starts with the node_id. */
    return memcmp(*a, *b, SELVA_NODE_ID_SIZE);
}
