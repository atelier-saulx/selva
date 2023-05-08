/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_RESOLVE
#define SELVA_RESOLVE

#include "selva_db.h"

struct SelvaHierarchy;
struct selva_string;

#define SELVA_RESOLVE_NODE_ID   0x40000000
#define SELVA_RESOLVE_ALIAS     0x20000000
#define SELVA_RESOLVE_FLAGS     0x60000000

/**
 * Resolve the first existing node_id from a list of aliases and node_ids.
 * @returns Index of the resolved index OR'ed with SELVA_RESOLVE_ALIAS if an alias was resolved;
 *          Index of the resovled index OR'ed with SELVA_RESOLVE_NODE_ID if a node_id was resolved;
 *          otherwise a Selva error is returned.
 */
int SelvaResolve_NodeId(
        struct SelvaHierarchy *hierarchy,
        struct selva_string **ids,
        size_t nr_ids,
        Selva_NodeId node_id);

#endif /* SELVA_RESOLVE */
