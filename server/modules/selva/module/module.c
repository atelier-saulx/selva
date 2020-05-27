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
    RedisModuleString *type = argv[i];
    RedisModuleString *field = argv[i + 1];
    RedisModuleString *value = argv[i + 2];

    size_t field_len;
    const char *field_str = RedisModule_StringPtrLen(field, &field_len);

    size_t type_len;
    const char *type_str = RedisModule_StringPtrLen(type, &type_len);

    if (*type_str == SELVA_MODIFY_ARG_INDEXED_VALUE || *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
      size_t value_len;
      const char *value_str = RedisModule_StringPtrLen(value, &value_len);
      int indexing_str_len =
          sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len + value_len;
      char indexing_str[indexing_str_len];
      SelvaModify_PrepareValueIndexPayload(indexing_str, id_str, id_len, field_str, field_len,
                                           value_str, value_len);
      SelvaModify_SendAsyncTask(indexing_str_len, indexing_str);
    }

    if (*type_str == SELVA_MODIFY_ARG_DEFAULT || *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
      RedisModule_HashSet(id_key, REDISMODULE_HASH_NX, field, value, NULL);
    } else if (*type_str == SELVA_MODIFY_ARG_OP_INCREMENT) {
      size_t value_len;
      const char *value_str = RedisModule_StringPtrLen(value, &value_len);

      struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;
      // TODO: call inline function
      if (incrementOpts->$default_len) {
        RedisModuleString *default_value = RedisModule_CreateString(ctx, incrementOpts->$default, incrementOpts->$default_len);
        RedisModule_HashSet(id_key, REDISMODULE_HASH_NX, field, default_value, NULL);
      }

      if (incrementOpts->$increment_len) {
        RedisModuleString *increment_value = RedisModule_CreateString(ctx, incrementOpts->$increment, incrementOpts->$increment_len);
        RedisModule_HashSet(id_key, REDISMODULE_HASH_NX, field, increment_value, NULL);
      }
    } else if (*type_str == SELVA_MODIFY_ARG_OP_REFERENCES) {
      size_t value_len;
      const char *value_str = RedisModule_StringPtrLen(value, &value_len);

      struct SelvaModify_OpReferences *referenceOpts = (struct SelvaModify_OpReferences *)value_str;
      // TODO: call inline function
      if (referenceOpts->$value_len) {
          // TODO: remove all
          for (unsigned int i = 0; i < referenceOpts->$value_len; i += 10) {
            // TODO: set
          }
      } else {
        if (referenceOpts->$add_len) {
          for (unsigned int i = 0; i < referenceOpts->$add_len; i += 10) {
            // TODO: sadd blabla
          }
        }

        if (referenceOpts->$delete_len) {
          for (unsigned int i = 0; i < referenceOpts->$delete_len; i += 10) {
            // TODO: srem blabla
          }
        }
      }

      // TODO: hierarchy
    } else {
      // normal set
      RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
    }

    int payload_len = sizeof(int32_t) + sizeof(struct SelvaModify_AsyncTask) + field_len;
    char payload_str[payload_len];

    // RedisModule_Call(ctx, "publish", field_str, "update");
    SelvaModify_PreparePublishPayload(payload_str, id_str, id_len, field_str, field_len);
    SelvaModify_SendAsyncTask(payload_len, payload_str);
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
