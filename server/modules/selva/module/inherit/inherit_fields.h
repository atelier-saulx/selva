#pragma once
#ifndef _SELVA_INHERIT_FIELDS_H_
#define _SELVA_INHERIT_FIELDS_H_

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaObjectAny;

/**
 * Get a plain field value.
 */
int Inherit_GetField(
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
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
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        struct RedisModuleString *full_field,
        const char *field_str,
        size_t field_len);

/**
 * Send a field value to the client in the find command format.
 */
int Inherit_SendFieldFind(
        RedisModuleCtx *ctx,
        SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const struct SelvaHierarchyNode *node,
        struct SelvaObject *obj,
        RedisModuleString *full_field,
        const char *field_str,
        size_t field_len);

#endif /* _SELVA_INHERIT_FIELDS_H_ */
