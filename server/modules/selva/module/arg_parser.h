#pragma once
#ifndef SELVA_ARG_PARSER
#define SELVA_ARG_PARSER

#include "selva.h"

/**
 * Parse an option with key string.
 * TXT INT_VALUE
 * @param value[out] pointer to a storage for the parser ssize_t value.
 * @param name a pointer to the key string.
 * @param txt a pointer to the key string argument.
 * @param num a pointer to the value string argument.
 */
int SelvaArgParser_IntOpt(ssize_t *value, const char *name, RedisModuleString *txt, RedisModuleString *num);

int SelvaArgParser_StrOpt(const char **value, const char *name, RedisModuleString *arg_key, RedisModuleString *arg_val);
int SelvaArgParser_SubscriptionId(Selva_SubscriptionId id, RedisModuleString *arg);
int SelvaArgParser_MarkerId(Selva_SubscriptionMarkerId *marker_id, RedisModuleString *arg);

#endif /* SELVA_ARG_PARSER */
