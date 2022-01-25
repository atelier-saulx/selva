#include <stddef.h>
#include "cdefs.h"
#include "rpn.h"

const char *rpn_str_error[] = {"", ""};

struct rpn_ctx *rpn_init(int nr_reg) { return NULL; }
void rpn_destroy(struct rpn_ctx *ctx) {}
enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s, size_t slen, unsigned flags) { return 1; }
struct rpn_expression *rpn_compile(const char *input) { return NULL; }
enum rpn_error rpn_bool(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, int *out) { return 1; }
enum rpn_error rpn_integer(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, long long *out) { return 1; }
enum rpn_error rpn_selvaset(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, struct SelvaSet *out) { return 1; }
