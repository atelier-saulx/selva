#include <cdefs.h>
#include "selva.h"
#include "subscriptions.h"

struct RedisModuleKey *SelvaSet_Open(
        struct RedisModuleCtx *ctx __unused,
        const char *id_str __unused, size_t id_len __unused,
        const char *field_str __unused) {
    return NULL;
}
