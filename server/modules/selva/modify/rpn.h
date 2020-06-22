#pragma once
#ifndef _MODIFY_RPN_H_
#define _MODIFY_RPN_H_

#define RPN_MAX_D 256

struct rpn_operand;

struct rpn_ctx {
    int depth;
    size_t reg_size;
    const char **reg;
    struct rpn_operand *stack[RPN_MAX_D];
};

void rpn_init(struct rpn_ctx *ctx, const char **reg, size_t reg_size);
int rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s);
int rpn_bool(struct rpn_ctx *ctx, const char *s, size_t s_len, int *out);
int rpn_integer(struct rpn_ctx *ctx, const char *s, size_t s_len, long long *out);

#endif /* _MODIFY_RPN_H_ */
