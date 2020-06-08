#pragma once
#ifndef SELVA_MODIFY_HIERARCHY
#define SELVA_MODIFY_HIERARCHY

#define SELVA_NODE_ID_SIZE 10

typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];
struct SelvaModify_Hierarchy;
typedef struct SelvaModify_Hierarchy SelvaModify_Hierarchy;

SelvaModify_Hierarchy *SelvaModify_NewHierarchy(void);
void SelvaModify_DestroyHierarchy(SelvaModify_Hierarchy *hierarchy);
int SelvaModify_SetHierarchy(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, size_t nr_parents, const Selva_NodeId *parents, size_t nr_children, const Selva_NodeId *children);
int SelvaModify_FindAncestors(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **ancestors);
int SelvaModify_FindDescendants(SelvaModify_Hierarchy *hierarchy, const Selva_NodeId id, Selva_NodeId **descendants);

#endif /* SELVA_MODIFY_HIERARCHY */
