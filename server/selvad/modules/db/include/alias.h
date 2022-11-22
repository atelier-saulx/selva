/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_ALIAS_H
#define SELVA_ALIAS_H

#include "selva_db.h"

struct RedisModuleKey;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaSet;

/**
 * Open the aliases key.
 */
struct RedisModuleKey *open_aliases_key(void);

/**
 * Remove aliases listed in set.
 * Caller must update the node aliases if necessary.
 */
int delete_aliases(struct RedisModuleKey *aliases_key, struct SelvaSet *set);

/**
 * Update alias into the aliases key and remove the previous alias.
 * Caller must set the alias to the new node.
 */
void update_alias(
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleKey *alias_key,
        const Selva_NodeId node_id,
        struct RedisModuleString *ref);

#endif /* SELVA_ALIAS_H */
