#include "../redismodule.h"
#include "../rmutil/util.h"
#include "../rmutil/strings.h"
#include "../rmutil/test_util.h"

#include "./id/id.h"
#include "./modify/modify.h"

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

  RedisModuleString *keyStr = RedisModule_CreateString(ctx, "flurpypants", strlen("flurpypants") * sizeof(char));
  RedisModuleString *val = RedisModule_CreateString(ctx, "hallo", strlen("hallo") * sizeof(char));
  RedisModuleKey *key = RedisModule_OpenKey(ctx, keyStr, REDISMODULE_WRITE);
  for (int i = 0; i < 10000; i++) {
    RedisModule_StringSet(key, val);
    // RedisModuleCallReply *r = RedisModule_Call(ctx, "publish", "x", "y");
  }

  RedisModule_CloseKey(key);
  RedisModuleString *reply =
    RedisModule_CreateString(ctx, "hallo", strlen("hallo") * sizeof(char));
  RedisModule_ReplyWithString(ctx, reply);
  return REDISMODULE_OK;

}

// id, type, key, value [, ... type, key, value]]
int SelvaCommand_Modify(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_AutoMemory(ctx);

  RedisModuleString *id = argv[1];
  size_t id_size;
  const char *id_str = RedisModule_StringPtrLen(id, &id_size);

  if (id_size == 2) {
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

    RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);

    // TODO: prepare indexing

    // prepare publish
    size_t field_size;
    const char *field_str = RedisModule_StringPtrLen(field, &field_size);
    int payload_size = 7 + 1 + id_size + 1 + field_size + 1 + 6;
    char payload_str[payload_size];
    char *payload_iterator = payload_str;
    memcpy(payload_iterator, "publish", 7);
    payload_iterator += 7;
    memcpy(payload_iterator, " ", 1);
    payload_iterator++;
    memcpy(payload_iterator, id_str, id_size);
    payload_iterator += id_size;
    memcpy(payload_iterator, ".", 1);
    payload_iterator++;
    memcpy(payload_iterator, field_str, field_size);
    payload_iterator += field_size;
    memcpy(payload_iterator, " ", 1);
    payload_iterator++;
    memcpy(payload_iterator, "update", 6);

    // publish
    SelvaModify_SendAsyncTask(payload_size, payload_str, 3);
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

  if (RedisModule_CreateCommand(ctx, "selva.id", SelvaCommand_GenId, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }


  if (RedisModule_CreateCommand(ctx, "selva.modify", SelvaCommand_Modify, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "selva.flurpypants", SelvaCommand_Flurpy, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
