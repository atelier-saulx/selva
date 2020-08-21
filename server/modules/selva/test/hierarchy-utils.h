#pragma once
#ifndef HIERARCHY_UTILS_H
#define HIERARCHY_UTILS_H

#include <stddef.h>
#include "cdefs.h"
#include "redis-rdb.h"
#include "hierarchy.h"

extern Selva_NodeId HIERARCHY_RDB_EOF __nonstring;
extern SelvaModify_Hierarchy *hierarchy;
extern Selva_NodeId *findRes;
extern RedisModuleIO *io;

int Selva_NodeId_Compare(const void *a, const void *b);
void SelvaNodeId_SortRes(size_t len);
char *SelvaNodeId_GetRes(size_t i);
void SelvaNodeId_copy2buf(char buf[SELVA_NODE_ID_SIZE + 1], const Selva_NodeId id);

#endif /* HIERARCHY_UTILS_H */
