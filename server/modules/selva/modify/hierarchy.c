#include "./hierarchy.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "../redismodule.h"

typedef struct _SelvaModify_HierarchyNode {
  char id[10];
  uint8_t traversal_counter;
  Vector *parents;
  Vector *children;
} SelvaModify_HierarchyNode;


int _Vector_BS_Compare(const void * restrict a_raw, const void * restrict b_raw) {
  const SelvaModify_HierarchyNode *a = (const SelvaModify_HierarchyNode *)a_raw;
  const SelvaModify_HierarchyNode *b = (const SelvaModify_HierarchyNode *)b_raw;

  return strncmp(a->id, b->id, 10);
}

void SelvaModify_SetHierarchy() {

}
