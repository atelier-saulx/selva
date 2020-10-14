#pragma once
#ifndef SELVA_OBJECT
#define SELVA_OBJECT

struct SelvaObject;
struct RedisModuleString;

int SelvaObject_Key2Obj(RedisModuleKey *key, struct SelvaObject **out);
int SelvaObject_DelKey(struct SelvaObject *obj, const RedisModuleString *key_name);
int SelvaObject_Exists(struct SelvaObject *obj, const RedisModuleString *key_name);
int SelvaObject_GetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString **out);
int SelvaObject_SetStr(struct SelvaObject *obj, const RedisModuleString *key_name, RedisModuleString *value);

#endif /* SELVA_OBJECT */
