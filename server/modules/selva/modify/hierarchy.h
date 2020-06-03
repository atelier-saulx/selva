#pragma once
#ifndef SELVA_MODIFY_HIERARCHY
#define SELVA_MODIFY_HIERARCHY

#define SELVA_NODE_ID_SIZE 10

typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];

int SelvaModify_SetHierarchy(Selva_NodeId id, size_t nr_parents, Selva_NodeId *parents, size_t nr_children, Selva_NodeId *children);
int SelvaModify_FindParents(Selva_NodeId id, Selva_NodeId **parents);
int SelvaModify_FindChildren(Selva_NodeId id, Selva_NodeId **children);

#endif /* SELVA_MODIFY_HIERARCHY */
