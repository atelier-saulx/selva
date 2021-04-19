#include "cdefs.h"
#include "selva.h"
#include "selva_object.h"
#include "selva_node.h"

int SelvaNode_Delete(
        struct RedisModuleCtx *ctx __unused,
        struct RedisModuleString *id __unused) {
    return 0;
}
