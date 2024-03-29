/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "redismodule.h"
#include "selva.h"
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

int SelvaSVectorComparator_RMS(const void ** restrict ap, const void ** restrict bp) {
    const RedisModuleString *a = *(const RedisModuleString **)ap;
    const RedisModuleString *b = *(const RedisModuleString **)bp;
    TO_STR(a, b);

    const ssize_t len_diff = a_len - b_len;
    if (len_diff != 0) {
        return len_diff < 0 ? -1 : 1;
    }

    return memcmp(a_str, b_str, a_len);
}

int SelvaSVectorComparator_Node(const void ** restrict a, const void ** restrict b) {
    /* We expect that the node struct starts with the node_id. */
    return memcmp(*a, *b, SELVA_NODE_ID_SIZE);
}
