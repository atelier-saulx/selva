#pragma once
#ifndef _MODIFY_RPN_H_
#define _MODIFY_RPN_H_

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
    RPN_ERR_NECESS,     /*!< A necessarily truthy condition failed. */
};

struct rpn_operand;
struct RedisModuleCtx;
struct RedisModuleKey;
struct RedisModuleString;

struct rpn_ctx {
    int depth;
    int nr_reg;
    struct RedisModuleCtx *redis_ctx;
    struct RedisModuleKey *redis_hkey; /*!< Redis hash key of the current node. */
    struct SelvaObject *obj; /*!< Selva object of the current node. */
    struct RedisModuleString *rms_id;  /*!< This holds the id of redis_hkey. */
    struct RedisModuleString *rms_field;  /*!< This holds the name of the currently accessed field. */
    struct rpn_operand **reg;
    struct rpn_operand *stack[RPN_MAX_D];
};

typedef char rpn_token[RPN_MAX_TOKEN_SIZE];

/*
 * Free register values after unref.
 */
#define RPN_SET_REG_FLAG_RMFREE 0x01

extern const char *rpn_str_error[11];

struct rpn_ctx *rpn_init(struct RedisModuleCtx *redis_ctx, int nr_reg);
void rpn_destroy(struct rpn_ctx *ctx);
enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s, size_t slen, unsigned flags);
enum rpn_error rpn_set_reg_rm(struct rpn_ctx *ctx, size_t i, struct RedisModuleString *rms);
rpn_token *rpn_compile(const char *input, size_t len);
enum rpn_error rpn_bool(struct rpn_ctx *ctx, const rpn_token *expr, int *out);
enum rpn_error rpn_double(struct rpn_ctx *ctx, const rpn_token *expr, double *out);
enum rpn_error rpn_integer(struct rpn_ctx *ctx, const rpn_token *expr, long long *out);

#endif /* _MODIFY_RPN_H_ */
