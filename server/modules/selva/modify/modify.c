#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "redismodule.h"

#include "hierarchy.h"
#include "modify.h"

int SelvaModify_ModifySet(
  RedisModuleCtx *ctx,
  RedisModuleKey *id_key,
  const char *id_str,
  size_t id_len,
  RedisModuleString *field,
  const char *field_str,
  size_t field_len,
  struct SelvaModify_OpSet *setOpts
) {
  // add in the hash that it's a set/references field
  RedisModuleString *set_field_identifier = RedisModule_CreateString(ctx, "___selva_$set", 13);
  RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, set_field_identifier, NULL);

  RedisModuleString *set_key_name = RedisModule_CreateStringPrintf(ctx, "%.*s%c%.*s", id_len, id_str, '.', field_len, field_str);
  RedisModuleKey *set_key = RedisModule_OpenKey(ctx, set_key_name, REDISMODULE_WRITE);

  if (!set_key) {
    return REDISMODULE_ERR;
  }

  if (setOpts->$value_len) {
    RedisModule_UnlinkKey(set_key);

    if (setOpts->is_reference) {
      for (unsigned int i = 0; i < setOpts->$value_len; i += SELVA_NODE_ID_SIZE) {
        RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$value + i, SELVA_NODE_ID_SIZE);
        RedisModule_ZsetAdd(set_key, 0, ref, NULL);
      }
    } else {
      char *ptr = setOpts->$value;
      for (size_t i = 0; i < setOpts->$value_len; ) {
        unsigned long part_len = strlen(ptr);

        RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
        RedisModule_ZsetAdd(set_key, 0, ref, NULL);

        // +1 to skip the nullbyte
        ptr += part_len + 1;
        i += part_len + 1;
      }
    }
  } else {
    if (setOpts->$add_len) {
      if (setOpts->is_reference) {
        for (unsigned int i = 0; i < setOpts->$add_len; i += SELVA_NODE_ID_SIZE) {
          RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$add + i, SELVA_NODE_ID_SIZE);
          // TODO: check if anything was actually added or not for hierarchy
          RedisModule_ZsetAdd(set_key, 0, ref, NULL);
        }
      } else {
        char *ptr = setOpts->$add;
        for (size_t i = 0; i < setOpts->$add_len; ) {
          unsigned long part_len = strlen(ptr);

          RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
          RedisModule_ZsetAdd(set_key, 0, ref, NULL);

          // +1 to skip the nullbyte
          ptr += part_len + 1;
          i += part_len + 1;
        }
      }
    }

    if (setOpts->$delete_len) {
      if (setOpts->is_reference) {
        for (unsigned int i = 0; i < setOpts->$delete_len; i += SELVA_NODE_ID_SIZE) {
          RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$delete + i, SELVA_NODE_ID_SIZE);
          // TODO: check if anything was actually removed or not for hierarchy
          RedisModule_ZsetRem(set_key, ref, NULL);
        }
      } else {
        char *ptr = setOpts->$delete;
        for (size_t i = 0; i < setOpts->$delete_len; ) {
          unsigned long part_len = strlen(ptr);

          RedisModuleString *ref = RedisModule_CreateString(ctx, ptr, part_len);
          RedisModule_ZsetRem(set_key, ref, NULL);

          // +1 to skip the nullbyte
          ptr += part_len + 1;
          i += part_len + 1;
        }
      }
    }
  }

  RedisModule_CloseKey(set_key);
  return REDISMODULE_OK;
}

void SelvaModify_ModifyIncrement(
  RedisModuleCtx *ctx,
  RedisModuleKey *id_key,
  const char *id_str,
  size_t id_len,
  RedisModuleString *field,
  const char *field_str,
  size_t field_len,
  RedisModuleString *current_value,
  const char *current_value_str,
  size_t current_value_len,
  struct SelvaModify_OpIncrement *incrementOpts
) {
  int num = current_value == NULL
    ? incrementOpts->$default
    : strtol(current_value_str, NULL, SELVA_NODE_ID_SIZE);
  num += incrementOpts->$increment;

  int num_str_size = (int)ceil(log10(num));
  char increment_str[num_str_size];
  sprintf(increment_str, "%d", num);

  RedisModuleString *increment =
    RedisModule_CreateString(ctx, increment_str, num_str_size);
  RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, increment, NULL);

  if (incrementOpts->index) {
    SelvaModify_Index(id_str, id_len, field_str, field_len, increment_str, num_str_size);
  }
}
