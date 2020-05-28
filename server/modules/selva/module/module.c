#include <math.h>

#include "../redismodule.h"
#include "../rmutil/util.h"
#include "../rmutil/strings.h"
#include "../rmutil/test_util.h"

#include "./id/id.h"
#include "./modify/modify.h"
#include "./modify/async_task.h"

int SelvaCommand_GenId(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);

  if (argc > 2) {
    return RedisModule_WrongArity(ctx);
  }

  char hash_str[37];
  SelvaId_GenId("", hash_str);

  RedisModuleString *reply =
      RedisModule_CreateString(ctx, hash_str, strlen(hash_str) * sizeof(char));
  RedisModule_ReplyWithString(ctx, reply);
  return REDISMODULE_OK;
}

int SelvaCommand_Flurpy(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  // init auto memory for created strings
  RedisModule_AutoMemory(ctx);

  RedisModuleString *keyStr =
      RedisModule_CreateString(ctx, "flurpypants", strlen("flurpypants") * sizeof(char));
  RedisModuleString *val = RedisModule_CreateString(ctx, "hallo", strlen("hallo") * sizeof(char));
  RedisModuleKey *key = RedisModule_OpenKey(ctx, keyStr, REDISMODULE_WRITE);
  for (int i = 0; i < 10000; i++) {
    RedisModule_StringSet(key, val);
    // RedisModuleCallReply *r = RedisModule_Call(ctx, "publish", "x", "y");
  }

  RedisModule_CloseKey(key);
  RedisModuleString *reply = RedisModule_CreateString(ctx, "hallo", strlen("hallo") * sizeof(char));
  RedisModule_ReplyWithString(ctx, reply);
  return REDISMODULE_OK;
}

// TODO: clean this up
// id, type, key, value [, ... type, key, value]]
int SelvaCommand_Modify(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx);

  RedisModuleString *id = argv[1];
  size_t id_len;
  const char *id_str = RedisModule_StringPtrLen(id, &id_len);

  if (id_len == 2) {
    char hash_str[37];
    SelvaId_GenId(id_str, hash_str);
    id_str = hash_str;
    id = RedisModule_CreateString(ctx, hash_str, strlen(hash_str) * sizeof(char));
  }

  RedisModuleKey *id_key = RedisModule_OpenKey(ctx, id, REDISMODULE_WRITE);
  for (int i = 2; i < argc; i += 3) {
    bool publish = true;
    RedisModuleString *type = argv[i];
    RedisModuleString *field = argv[i + 1];
    RedisModuleString *value = argv[i + 2];

    size_t field_len;
    const char *field_str = RedisModule_StringPtrLen(field, &field_len);

    size_t type_len;
    const char *type_str = RedisModule_StringPtrLen(type, &type_len);

    size_t value_len;
    const char *value_str = RedisModule_StringPtrLen(value, &value_len);

    size_t current_value_len;
    RedisModuleString *current_value;
    RedisModule_HashGet(id_key, REDISMODULE_HASH_NONE, field, &current_value, NULL);
    const char *current_value_str;

    if (current_value == NULL) {
      current_value = NULL;
      current_value_len = 0;
    } else {
      current_value_str = RedisModule_StringPtrLen(current_value, &current_value_len);
    }

    if (*type_str != SELVA_MODIFY_ARG_OP_INCREMENT && *type_str != SELVA_MODIFY_ARG_OP_REFERENCES &&
        current_value_len == value_len && memcmp(current_value, value, current_value_len) == 0) {
      // printf("Current value is equal to the specified value for key %s and value %s\n", field_str,
      //        value_str);
      continue;
    }

    if (*type_str == SELVA_MODIFY_ARG_INDEXED_VALUE ||
        *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
      int indexing_str_len =
          sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len + value_len;
      char indexing_str[indexing_str_len];
      SelvaModify_PrepareValueIndexPayload(indexing_str, id_str, id_len, field_str, field_len,
                                           value_str, value_len);
      SelvaModify_SendAsyncTask(indexing_str_len, indexing_str);
    }

    if (*type_str == SELVA_MODIFY_ARG_DEFAULT || *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
      if (current_value != NULL) {
        publish = false;
      } else {
        RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
      }
    } else if (*type_str == SELVA_MODIFY_ARG_OP_INCREMENT) {
      struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;

      int num = current_value == NULL ? incrementOpts->$default : atoi(current_value_str);
      num += incrementOpts->$increment;

      int num_str_size = (int)ceil(log10(num));
      char increment_str[num_str_size];
      sprintf(increment_str, "%d", num);

      RedisModuleString *increment =
        RedisModule_CreateString(ctx, increment_str, num_str_size);
      RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, increment, NULL);

      if (incrementOpts->index) {
        int indexing_str_len =
          sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len + num_str_size;
        char indexing_str[indexing_str_len];
        SelvaModify_PrepareValueIndexPayload(indexing_str, id_str, id_len, field_str, field_len,
            increment_str, num_str_size);
        SelvaModify_SendAsyncTask(indexing_str_len, indexing_str);
      }
    } else if (*type_str == SELVA_MODIFY_ARG_OP_REFERENCES) {
      struct SelvaModify_OpReferences *referenceOpts = (struct SelvaModify_OpReferences *)value_str;
      SelvaModify_OpReferences_align(referenceOpts);

      // add in the hash that it's a set/references field
      RedisModuleString *set_field_identifier = RedisModule_CreateString(ctx, "___selva_$set", 13);
      RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, set_field_identifier, NULL);

      // TODO: optimize that this is copied only once, we now do this for sending publish and indexing also
      char set_key_str[id_len + 1 + field_len];
      memcpy(set_key_str, id_str, id_len);
      memcpy(set_key_str + id_len, ".", 1);
      memcpy(set_key_str + id_len + 1, field_str, field_len);
      RedisModuleString *_set_key = RedisModule_CreateString(ctx, set_key_str, id_len + 1 + field_len);
      RedisModuleKey *set_key = RedisModule_OpenKey(ctx, _set_key, REDISMODULE_WRITE);

      if (referenceOpts->$value_len) {
        RedisModule_DeleteKey(set_key);

        for (unsigned int i = 0; i < referenceOpts->$value_len; i += 10) {
          RedisModuleString *ref = RedisModule_CreateString(ctx, referenceOpts->$value + i, 10);
          RedisModule_ZsetAdd(set_key, 0, ref, NULL);
        }
      } else {
        if (referenceOpts->$add_len) {
          for (unsigned int i = 0; i < referenceOpts->$add_len; i += 10) {
            RedisModuleString *ref = RedisModule_CreateString(ctx, referenceOpts->$add + i, 10);
            // TODO: check if anything was actually added or not for hierarchy
            RedisModule_ZsetAdd(set_key, 0, ref, NULL);
          }
        }

        if (referenceOpts->$delete_len) {
          for (unsigned int i = 0; i < referenceOpts->$delete_len; i += 10) {
            RedisModuleString *ref = RedisModule_CreateString(ctx, referenceOpts->$delete + i, 10);
            // TODO: check if anything was actually removed or not for hierarchy
            RedisModule_ZsetRem(set_key, ref, NULL);
          }
        }
      }

      RedisModule_CloseKey(set_key);

      // TODO: hierarchy
    } else {
      // normal set
      RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
    }

    if (publish) {
      int payload_len = sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len;
      char payload_str[payload_len];

      SelvaModify_PreparePublishPayload(payload_str, id_str, id_len, field_str, field_len);
      SelvaModify_SendAsyncTask(payload_len, payload_str);
    }
  }

  RedisModule_CloseKey(id_key);

  RedisModule_ReplyWithString(ctx, id);

  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx) {

  // Register the module itself
  if (RedisModule_Init(ctx, "selva", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "selva.id", SelvaCommand_GenId, "readonly", 1, 1, 1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "selva.modify", SelvaCommand_Modify, "readonly", 1, 1, 1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "selva.flurpypants", SelvaCommand_Flurpy, "readonly", 1, 1,
                                1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
