#include <math.h>

#include "cdefs.h"
#include "redismodule.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include "rmutil/test_util.h"

#include "id/id.h"
#include "modify/modify.h"
#include "modify/async_task.h"

#define TO_STR_1(_var) \
  size_t _var##_len; \
  const char * _var##_str = RedisModule_StringPtrLen(_var, & _var##_len);

#define TO_STR_2(_var, ...) \
  TO_STR_1(_var) \
  TO_STR_1(__VA_ARGS__)

#define TO_STR_3(_var, ...) \
  TO_STR_1(_var) \
  TO_STR_2(__VA_ARGS__)

#define TO_STR_4(_var, ...) \
  TO_STR_1(_var) \
  TO_STR_3(__VA_ARGS__)

#define TO_STR_5(_var, ...) \
  TO_STR_1(_var) \
  TO_STR_5(__VA_ARGS__)

#define TO_STR_6(_var, ...) \
  TO_STR_1(_var) \
  TO_STR_6(__VA_ARGS__)

#define TO_STR(...) \
    CONCATENATE(TO_STR_, UTIL_NARG(__VA_ARGS__))(__VA_ARGS__)

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

    TO_STR_3(type, field, value);

    size_t current_value_len = 0;
    RedisModuleString *current_value;
    RedisModule_HashGet(id_key, REDISMODULE_HASH_NONE, field, &current_value, NULL);
    const char *current_value_str = NULL;

    if (current_value != NULL) {
      current_value_str = RedisModule_StringPtrLen(current_value, &current_value_len);
    }

    if (*type_str != SELVA_MODIFY_ARG_OP_INCREMENT && *type_str != SELVA_MODIFY_ARG_OP_SET &&
        current_value_len == value_len && memcmp(current_value, value, current_value_len) == 0) {
      // printf("Current value is equal to the specified value for key %s and value %s\n", field_str,
      //        value_str);
      continue;
    }

    if (*type_str == SELVA_MODIFY_ARG_OP_INCREMENT) {
      struct SelvaModify_OpIncrement *incrementOpts = (struct SelvaModify_OpIncrement *)value_str;
      SelvaModify_ModifyIncrement(ctx, id_key, id_str, id_len, field, field_str, field_len,
          current_value, current_value_str, current_value_len, incrementOpts);
    } else if (*type_str == SELVA_MODIFY_ARG_OP_SET) {
      struct SelvaModify_OpSet *setOpts = (struct SelvaModify_OpSet *)value_str;
      SelvaModify_OpSet_align(setOpts);
      SelvaModify_ModifySet(ctx, id_key, id_str, id_len, field, field_str, field_len, setOpts);
      // TODO: hierarchy
    } else {
      if (*type_str == SELVA_MODIFY_ARG_INDEXED_VALUE ||
          *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
        SelvaModify_Index(id_str, id_len, field_str, field_len, value_str, value_len);
      }

      if (*type_str == SELVA_MODIFY_ARG_DEFAULT || *type_str == SELVA_MODIFY_ARG_DEFAULT_INDEXED) {
        if (current_value != NULL) {
          publish = false;
        } else {
          RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
        }
      } else {
        RedisModule_HashSet(id_key, REDISMODULE_HASH_NONE, field, value, NULL);
      }
    }

    if (publish) {
      SelvaModify_Publish(id_str, id_len, field_str, field_len);
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
