#include "./hierarchy.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "../redismodule.h"

struct _SelvaModify_HierarchyNode;

typedef struct _Vector {
  size_t full_len;
  size_t read_len;

  struct _SelvaModify_HierarchyNode **data;
} Vector;

typedef struct _SelvaModify_HierarchyNode {
  char id[10];
  uint8_t traversal_counter;
  Vector *parents;
  Vector *children;
} SelvaModify_HierarchyNode;


int _Vector_BS_Compare(const void *a_raw, const void *b_raw) {
  const SelvaModify_HierarchyNode *a = (SelvaModify_HierarchyNode *)a_raw;
  const SelvaModify_HierarchyNode *b = (SelvaModify_HierarchyNode *)b_raw;

  return strncmp(a->id, b->id, 10);
}

Vector *Vector_New(size_t initial_len) {
  Vector *vec = RedisModule_Alloc(sizeof(Vector));
  vec->full_len = initial_len;
  vec->read_len = 0;
  vec->data = RedisModule_Alloc(sizeof(SelvaModify_HierarchyNode *) * initial_len);
}

void Vector_Insert(Vector *vec, SelvaModify_HierarchyNode *el) {
  if (vec->read_len >= vec->full_len) {
    vec->full_len *= 2;
    SelvaModify_HierarchyNode **new_data = RedisModule_Realloc(vec->data, sizeof(SelvaModify_HierarchyNode *) * vec->full_len);
    if (new_data == NULL) {
      new_data = RedisModule_Alloc(sizeof(char *) * vec->full_len);
      RedisModule_Free(vec->data);
      vec->data = new_data;
    }
  }

  SelvaModify_HierarchyNode *insertion_ptr = bsearch(el->id, vec->data, vec->read_len, sizeof(SelvaModify_HierarchyNode *), &_Vector_BS_Compare);
  memmove(&insertion_ptr + 1, &insertion_ptr, vec->read_len);
  vec->data[&insertion_ptr - vec->data] = el;
}

void SelvaModify_SetHierarchy() {

}
