#pragma once
#ifndef _RPN_H_
#define _RPN_H_

/**
 * Maximum size of a single token in an RPN expression.
 * An operand in a compiled expression cannot exceed this length.
 * The size of a literal operand is not limited by this setting.
 */
#define RPN_MAX_TOKEN_SIZE              6

enum rpn_error {
    RPN_ERR_OK = 0,     /*!< No error. */
    RPN_ERR_ENOMEM,     /*!< Out of memory. */
    RPN_ERR_NOTSUP,     /*!< Operation not supported. */
    RPN_ERR_ILLOPC,     /*!< Illegal operator. */
    RPN_ERR_ILLOPN,     /*!< Illegal operand. */
    RPN_ERR_BADSTK,     /*!< Stack error. */
    RPN_ERR_TYPE,       /*!< Type error. */
    RPN_ERR_BNDS,       /*!< Register index out of bounds. */
    RPN_ERR_NPE,        /*!< A NULL pointer was encountered. */
    RPN_ERR_NAN,        /*!< Not a number. */
    RPN_ERR_DIV,        /*!< Divide by zero. */
    RPN_ERR_BREAK,      /*!< Breaking condition. Never returned. */
    RPN_ERR_LAST,
};

struct rpn_operand;
struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;
struct SelvaSet;
struct SelvaModify_HierarchyNode;
struct rpn_operand;

struct rpn_ctx {
    int depth;
    int nr_reg;
    struct SelvaModify_HierarchyNode *node; /*!< A pointer to the current hierarchy node set with rpn_set_hierarchy_node(). */
    struct RedisModuleKey *redis_key; /*!< Redis key of the current node object. */
    struct SelvaObject *obj; /*!< Selva object of the current node. */
    struct RedisModuleString *rms_id;  /*!< This holds the id of redis_hkey. */
    struct RedisModuleString *rms_field;  /*!< This holds the name of the currently accessed field. */
    struct rpn_operand **reg;
    struct rpn_operand *stack[RPN_MAX_D];
};

typedef char rpn_token[RPN_MAX_TOKEN_SIZE];

/**
 * A reusable compilation result of the string formatted RPN expression.
 */
struct rpn_expression {
    rpn_token *expression;
    struct rpn_operand *input_literal_reg[RPN_MAX_D];
};

/*
 * Free register values after unref.
 */
#define RPN_SET_REG_FLAG_RMFREE 0x01

extern const char *rpn_str_error[RPN_ERR_LAST];

struct rpn_ctx *rpn_init(int nr_reg);
void rpn_destroy(struct rpn_ctx *ctx);

/**
 * Set the hierarchy node pointer in the RPN context.
 * Setting the node pointer enables some operands operating on the hierarchy but
 * it's not necessary to set this if those operands are not needed.
 * An operand requiring a node pointer will return RPN_ERR_ILLOPN if the pointer
 * is not set.
 */
static inline void rpn_set_hierarchy_node(struct rpn_ctx *ctx, struct SelvaModify_HierarchyNode *node) {
    ctx->node = node;
}

enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s, size_t slen, unsigned flags);
enum rpn_error rpn_set_reg_rm(struct rpn_ctx *ctx, size_t i, struct RedisModuleString *rms);
enum rpn_error rpn_set_reg_slvset(struct rpn_ctx *ctx, size_t i, struct SelvaSet *set, unsigned flags);
struct rpn_expression *rpn_compile(const char *input, size_t len);
void rpn_destroy_expression(struct rpn_expression *expr);
enum rpn_error rpn_bool(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, int *out);
enum rpn_error rpn_double(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, double *out);
enum rpn_error rpn_integer(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, long long *out);
enum rpn_error rpn_selvaset(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, struct SelvaSet *out);

#endif /* _RPN_H_ */
