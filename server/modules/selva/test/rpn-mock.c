#include <stddef.h>

const char *rpn_str_error[] = {""};

void rpn_init(void *ctx, void *redis_ctx, const char **reg, int nr_reg) { }
int rpn_set_reg(void *ctx, size_t i, const char *s) { return 0; }
int rpn_bool(void *ctx, const char *s, size_t s_len, int *out) { return 0; }
