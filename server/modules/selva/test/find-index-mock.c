#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "traversal.h"
#include "find_index.h"

void SelvaFindIndex_Init(struct RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy) {
    return;
}

void SelvaFindIndex_Deinit(struct SelvaHierarchy *hierarchy) {
    return;
}

int SelvaFind_AutoIndex(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
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

