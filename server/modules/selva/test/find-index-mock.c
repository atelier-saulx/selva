#include <stddef.h>
#include <sys/types.h>
#include "traversal.h"
#include "find_index.h"

int SelvaFindIndex_Init(struct RedisModuleCtx *ctx, struct SelvaModify_Hierarchy *hierarchy) {
    return 0;
}

void SelvaFindIndex_Deinit(struct SelvaModify_Hierarchy *hierarchy) {
    return;
}

int SelvaFind_AutoIndex(
        struct RedisModuleCtx *ctx,
        struct SelvaModify_Hierarchy *hierarchy,
        enum SelvaTraversal dir, struct RedisModuleString *dir_expression_str,
        const Selva_NodeId node_id,
        struct RedisModuleString *filter,
        struct SelvaFindIndexControlBlock **icb_out,
        struct SelvaSet **out) {
    return 0;
}

void SelvaFind_Acc(struct SelvaFindIndexControlBlock * restrict icb, size_t acc_take, size_t acc_tot) {
    return;
}

