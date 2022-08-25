#pragma once
#ifndef _SELVA_INHERIT_H_
#define _SELVA_INHERIT_H_

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaHierarchy;
struct SelvaObjectAny;

/**
 * Inherit a field value for given node_id.
 * @param ctx is a pointer to the current Redis context.
 * @param hierarchy is a pointer to the hierarchy.
 * @param lang an optional lang list.
 * @param node_id is the starting node_id.
 * @param types is a list of types allowed for inherit.
 * @param nr_types is the number of ids in `types`.
 * @param field_name_str is a pointer to the field name.
 * @param field_name_len is the size of `field_name_str`.
 * @param[out] res is used to return the field value.
 */
int Inherit_FieldValue(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct RedisModuleString *lang,
        const Selva_NodeId node_id,
        const Selva_NodeType *types,
        size_t nr_types,
        const char *field_name_str,
        size_t field_name_len,
        struct SelvaObjectAny *res);

/**
 * Inherit fields and send them to the client in the find command format.
 * @param ctx is a pointer to the current Redis context.
 * @param hierarchy is a pointer to the hierarchy.
 * @param lang an optional lang list.
 * @param node_id is the starting node_id.
 * @param field_names is an array of pointers to field names.
 * @param Returns the number of fields sent.
 */
size_t Inherit_SendFields(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        RedisModuleString *lang,
        const Selva_NodeId node_id,
        RedisModuleString **field_names,
        size_t nr_field_names);

#endif /* _SELVA_INHERIT_H_ */
