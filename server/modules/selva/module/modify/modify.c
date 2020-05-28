#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../redismodule.h"

#include "./modify.h"

void SelvaModify_ModifySet(
  RedisModuleCtx *ctx,
  RedisModuleKey *id_key,
  const char *id_str,
  size_t id_len,
  RedisModuleString *field,
  const char *field_str,
  size_t field_len,
  struct SelvaModify_OpSet *setOpts
) {
  size_t id_field_len = id_len + 1 + field_len;
  char id_field_str[id_field_len];
  memcpy(id_field_str, id_str, id_len);
  memcpy(id_field_str + id_len, ".", 1);
  memcpy(id_field_str + id_len + 1, field_str, field_len);

  // add in the hash that it's a set/references field
  RedisModuleString *set_field_identifier = RedisModule_CreateString(ctx, "___selva_$set", 13);
  RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, set_field_identifier, NULL);

  RedisModuleString *_set_key = RedisModule_CreateString(ctx, id_field_str, id_field_len);
  RedisModuleKey *set_key = RedisModule_OpenKey(ctx, _set_key, REDISMODULE_WRITE);

  if (setOpts->$value_len) {
    RedisModule_DeleteKey(set_key);

    if (setOpts->is_reference) {
      for (unsigned int i = 0; i < setOpts->$value_len; i += 10) {
        RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$value + i, 10);
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
        for (unsigned int i = 0; i < setOpts->$add_len; i += 10) {
          RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$add + i, 10);
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
        for (unsigned int i = 0; i < setOpts->$delete_len; i += 10) {
          RedisModuleString *ref = RedisModule_CreateString(ctx, setOpts->$delete + i, 10);
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
}
