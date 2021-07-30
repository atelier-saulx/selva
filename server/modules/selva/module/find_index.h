#pragma once
#ifndef _FIND_INDEX_H_
#define _FIND_INDEX_H_

struct RedisModuleCtx;
struct RedisModuleString;
struct SelvaModify_Hierarchy;
struct SelvaSet;

/**
 * Check if an index exists for this query, update it, and get the indexing result set.
 * @param out is a SelvaSet of node_ids indexed for given clause.
 */
int SelvaFind_AutoIndex(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        enum SelvaTraversal dir, struct RedisModuleString *dir_expression_str,
        const Selva_NodeId node_id,
        RedisModuleString *filter,
        struct SelvaSet **out);

#endif /* _FIND_INDEX_H_ */
