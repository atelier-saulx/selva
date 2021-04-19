#pragma once
#ifndef SELVA_ARG_PARSER
#define SELVA_ARG_PARSER

#include "selva.h"

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaObject;

struct SelvaArgParser_EnumType {
    char *name;
    int id;
};

/**
 * Parse an option with key string.
 * TXT INT_VALUE
 * @param value[out] pointer to a storage for the parser ssize_t value.
 * @param name a pointer to the key string.
 * @param txt a pointer to the key string argument.
 * @param num a pointer to the value string argument.
 */
int SelvaArgParser_IntOpt(
        ssize_t *value, const char *name,
        const struct RedisModuleString *txt,
        const struct RedisModuleString *num);

int SelvaArgParser_StrOpt(
        const char **value,
        const char *name,
        const struct RedisModuleString *arg_key,
        const struct RedisModuleString *arg_val);
int SelvaArgsParser_StringList(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString ***out,
        const char *name,
        const struct RedisModuleString *arg_key,
        const struct RedisModuleString *arg_val);
int SelvaArgsParser_StringSetList(
        struct RedisModuleCtx *ctx,
        struct SelvaObject **out,
        const char *name,
        const struct RedisModuleString *arg_key,
        const struct RedisModuleString *arg_val);
int SelvaArgParser_Enum(
        const struct SelvaArgParser_EnumType types[],
        const struct RedisModuleString *arg);
void SelvaArgParser_NodeId(
        Selva_NodeId node_id,
        const struct RedisModuleString *arg);
int SelvaArgParser_SubscriptionId(
        Selva_SubscriptionId id,
        const struct RedisModuleString *arg);
int SelvaArgParser_MarkerId(
        Selva_SubscriptionMarkerId *marker_id,
        const struct RedisModuleString *arg);

#endif /* SELVA_ARG_PARSER */
