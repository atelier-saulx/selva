#include <stddef.h>
#include "cdefs.h"

const char *rpn_str_error[] = {""};

void rpn_init(void *ctx __unused, void *redis_ctx __unused, const char **reg __unused, int nr_reg __unused) { }
int rpn_set_reg(void *ctx __unused, size_t i __unused, const char *s __unused) { return 0; }
int rpn_bool(void *ctx __unused, const char *s __unused, size_t s_len __unused, int *out __unused) { return 0; }
