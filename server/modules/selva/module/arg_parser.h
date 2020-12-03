#pragma once
#ifndef SELVA_ARG_PARSER
#define SELVA_ARG_PARSER

#include "selva.h"

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaObject;

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
        struct RedisModuleString *txt,
        struct RedisModuleString *num);

int SelvaArgParser_StrOpt(
        const char **value,
        const char *name,
        struct RedisModuleString *arg_key,
        struct RedisModuleString *arg_val);
int SelvaArgsParser_StringList(
        struct RedisModuleCtx *ctx,
        struct RedisModuleString ***out,
        const char *name,
        struct RedisModuleString *arg_key,
        struct RedisModuleString *arg_val);
int SelvaArgsParser_StringListList(
        struct RedisModuleCtx *ctx,
        struct SelvaObject **out,
        const char *name,
        struct RedisModuleString *arg_key,
        struct RedisModuleString *arg_val);
void SelvaArgParser_NodeId(
        Selva_NodeId node_id,
        struct RedisModuleString *arg);
int SelvaArgParser_SubscriptionId(
        Selva_SubscriptionId id,
        struct RedisModuleString *arg);
int SelvaArgParser_MarkerId(
        Selva_SubscriptionMarkerId *marker_id,
        struct RedisModuleString *arg);

#endif /* SELVA_ARG_PARSER */
