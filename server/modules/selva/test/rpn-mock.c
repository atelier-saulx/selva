#include <stddef.h>
#include "cdefs.h"
#include "rpn.h"

const char *rpn_str_error[] = {"", ""};

struct rpn_ctx *rpn_init(int nr_reg __unused) { return NULL; }
void rpn_destroy(struct rpn_ctx *ctx __unused) {}
void rpn_set_hierarchy_node(struct rpn_ctx *ctx, const struct SelvaHierarchyNode *node) {}
void rpn_set_obj(struct rpn_ctx *ctx, struct SelvaObject *obj) {}
enum rpn_error rpn_set_reg(struct rpn_ctx *ctx __unused, size_t i __unused, const char *s __unused, size_t slen __unused, unsigned flags __unused) { return 1; }
struct rpn_expression *rpn_compile(const char *input __unused) { return NULL; }
enum rpn_error rpn_bool(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused, const struct rpn_expression *expr __unused, int *out __unused) { return 1; }
enum rpn_error rpn_integer(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused, const struct rpn_expression *expr __unused, long long *out __unused) { return 1; }
enum rpn_error rpn_selvaset(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused, const struct rpn_expression *expr __unused, struct SelvaSet *out __unused) { return 1; }
