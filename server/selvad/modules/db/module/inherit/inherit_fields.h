/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_INHERIT_FIELDS_H_
#define _SELVA_INHERIT_FIELDS_H_

struct SelvaHierarchy;
struct SelvaObjectAny;
struct selva_server_response_out;
struct selva_string;

/**
 * Get a plain field value.
 */
int Inherit_GetField(
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        const char *field_str,
        size_t field_len,
        struct SelvaObjectAny *out);

/**
 * Send a field value to the client.
 * Particularly this function sends the node_id, field name, and field value in
 * the format expected by the client.
 */
int Inherit_SendField(
        struct selva_server_response_out *resp,
        struct SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        struct selva_string *full_field,
        const char *field_str,
        size_t field_len);

/**
 * Send a field value to the client in the find command format.
 */
int Inherit_SendFieldFind(
        struct selva_server_response_out *resp,
        SelvaHierarchy *hierarchy,
        struct selva_string *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        struct selva_string *full_field,
        const char *field_str,
        size_t field_len);

#endif /* _SELVA_INHERIT_FIELDS_H_ */
