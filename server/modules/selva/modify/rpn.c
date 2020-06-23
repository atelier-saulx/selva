#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "redismodule.h"
#include "hierarchy.h"
#include "rpn.h"

#define OPERAND(ctx, x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = pop(ctx); \
    if (!x) return RPN_ERR_BADSTK

#define RPN_ASSERTS 0

struct rpn_operand {
    unsigned pooled : 1;
    struct rpn_operand *next_free; /* Next free in pool */
    long long i;
    size_t s_size;
    char s[SELVA_NODE_ID_SIZE + 1];
};

static struct rpn_operand *small_operand_pool_next;
static struct rpn_operand small_operand_pool[SMALL_OPERAND_POOL_SIZE];

char *rpn_str_error[] = {
    "No error",
    "Operation not supported",
    "Illegal operator",
    "Illegal operand",
    "Stack error",
    "Type error",
    "Register index out of bounds",
    "Null pointer exception",
    "Not a number",
    "Divide by zero",
};

static void init_pool(void) __attribute__((constructor));
static void init_pool(void) {
    struct rpn_operand *prev = NULL;

    small_operand_pool_next = &small_operand_pool[0];

    for (int i = SMALL_OPERAND_POOL_SIZE - 1; i >= 0; i--) {
        small_operand_pool[i].next_free = prev;
        prev = &small_operand_pool[i];
    }
}

void rpn_init(struct rpn_ctx *ctx, RedisModuleCtx *redis_ctx, const char **reg, int nr_reg) {
    ctx->depth = 0;
    ctx->redis_ctx = redis_ctx;
    ctx->reg = reg;
    ctx->nr_reg = nr_reg;
}

enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s) {
    if (i >= (size_t)ctx->nr_reg) {
        return RPN_ERR_BNDS;
    }

    ctx->reg[i] = s;

    return RPN_ERR_OK;
}

static struct rpn_operand *alloc_rpn_operand(size_t s_len) {
    struct rpn_operand *v;

    if (s_len < SELVA_NODE_ID_SIZE && small_operand_pool_next) {
        v = small_operand_pool_next;
        small_operand_pool_next = v->next_free;

        memset(v, 0, sizeof(struct rpn_operand));
        v->pooled = 1;
    } else {
        v = RedisModule_Calloc(1, sizeof(struct rpn_operand) + SELVA_NODE_ID_SIZE - s_len);

        return v;
    }

    return v;
}

static void free_rpn_operand(void *p) {
    struct rpn_operand **pp = (struct rpn_operand **)p;
    struct rpn_operand *v;

    if (unlikely(!pp)) {
        return;
    }

    v = *pp;
    if (!v) {
        return;
    }

    if (v->pooled) {
        struct rpn_operand *prev = small_operand_pool_next;

        /*
         * Put a pooled operand back to the pool.
         */
        small_operand_pool_next = v;
        small_operand_pool_next->next_free = prev;
    } else {
        RedisModule_Free(v);
    }
}

static void push(struct rpn_ctx *ctx, struct rpn_operand *v) {
	if (ctx->depth >= RPN_MAX_D) {
        /* TODO return stack overflow error */
        return;
    }

	ctx->stack[ctx->depth++] = v;
}

/* TODO Handle errors */
static void push_int_result(struct rpn_ctx *ctx, long long x) {
    struct rpn_operand *v = alloc_rpn_operand(0);

    v->i = x;
    push(ctx, v);
}

/* TODO Handle errors */
static void push_string_result(struct rpn_ctx *ctx, const char *s, size_t slen) {
    struct rpn_operand *v = alloc_rpn_operand(slen);

    v->s_size = slen + 1;
    strncpy(v->s, s, slen);
    v->s[slen] = '\0';
    push(ctx, v);
}

static struct rpn_operand *pop(struct rpn_ctx *ctx) {
	if (!ctx->depth) {
        return NULL;
    }

	return ctx->stack[--ctx->depth];
}

static void clear_stack(struct rpn_ctx *ctx) {
    struct rpn_operand *v;

    while ((v = pop(ctx))) {
        free_rpn_operand(&v);
    }
}

static int to_bool(struct rpn_operand *v) {
    return (v->s_size > 0 && v->s[0] != '\0') || !!v->i;
}

static enum rpn_error rpn_op_get_reg(struct rpn_ctx *ctx) {
    OPERAND(ctx, vType);
    OPERAND(ctx, a);
    const int type = vType->i;
    const size_t i = a->i;

    if (i >= (size_t)ctx->nr_reg) {
        fprintf(stderr, "RPN: Register index out of bounds: %zu\n", i);
        return RPN_ERR_BNDS;
    }

    const char *s = ctx->reg[i];

    if (!s) {
        fprintf(stderr, "RPN: Register value is a NULL pointer: %zu\n", i);
        return RPN_ERR_NPE;
    }

    if (type == 0) {
        char *e;
        long long v = strtoull(s, &e, 10);

        if (e == s) {
            fprintf(stderr, "RPN: Register value is not a number: %zu\n", i);
            return RPN_ERR_NAN;
        }

        push_int_result(ctx, v);
    } else if (type == 1) {
        push_string_result(ctx, s, strlen(s));
    } else {
        fprintf(stderr, "RPN: Unknown read type: %d\n", type);
        return RPN_ERR_TYPE;
    }

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_add(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i + b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_sub(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i - b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_div(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    long long d = b->i;

    if (d == 0) {
        return RPN_ERR_DIV;
    }

    push_int_result(ctx, a->i / d);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_mul(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i * b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_rem(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    long long d = b ->i;

    if (d == 0) {
        return RPN_ERR_DIV;
    }

    push_int_result(ctx, a->i % d);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_eq(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i == b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_ne(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i != b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_lt(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i < b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_gt(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i > b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_le(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i <= b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_ge(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, a->i >= b->i);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_not(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    push_int_result(ctx, !to_bool(a));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_and(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, to_bool(a) && to_bool(b));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_or(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, to_bool(a) || to_bool(b));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_xor(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, to_bool(a) ^ to_bool(b));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_in(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    /* TODO */

    return RPN_ERR_NOTSUP;
}

static enum rpn_error rpn_op_typeof(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    char t[SELVA_NODE_TYPE_SIZE];

    if (!a->s || a->s_size < SELVA_NODE_ID_SIZE) {
        return RPN_ERR_TYPE;
    }

#if SELVA_NODE_TYPE_SIZE != 2
#error Expected SELVA_NODE_TYPE_SIZE to be 2
#endif
    t[0] = a->s[0];
    t[1] = a->s[1];

    push_string_result(ctx, t, sizeof(t));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_strcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, !strcmp(a->s, b->s));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_idcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    push_int_result(ctx, !memcmp(a->s, b->s, SELVA_NODE_ID_SIZE));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_cidcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

#if RPN_ASSERTS
    assert(ctx->reg[0]);
#endif

    const char *cid = ctx->reg[0];
    const char t[SELVA_NODE_TYPE_SIZE] = { cid[0], cid[1] };

#if SELVA_NODE_TYPE_SIZE != 2
#error Expected SELVA_NODE_TYPE_SIZE to be 2
#endif

    push_int_result(ctx, !memcmp(a->s, t, SELVA_NODE_TYPE_SIZE));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_getfld(struct rpn_ctx *ctx) {
    OPERAND(ctx, field);
    RedisModuleString *cid;
    RedisModuleKey *id_key;
    RedisModuleString *value;

#if RPN_ASSERTS
    assert(ctx->reg[0]);
#endif

    cid = RedisModule_CreateString(NULL, ctx->reg[0], SELVA_NODE_ID_SIZE);
    id_key = RedisModule_OpenKey(ctx->redis_ctx, cid, REDISMODULE_READ);
    if (!id_key) {
        push_string_result(ctx, "", 0);
        goto out;
    }

    RedisModule_HashGet(id_key, REDISMODULE_HASH_CFIELDS, field->s, &value, NULL);
    if (value) {
        size_t value_len;
        const char *value_str;

        value_str = RedisModule_StringPtrLen(value, &value_len);
        push_string_result(ctx, value_str, value_len);

        RedisModule_FreeString(ctx->redis_ctx, value);
    } else {
        push_string_result(ctx, "", 0);
    }

out:
    RedisModule_CloseKey(id_key);
    RedisModule_FreeString(ctx->redis_ctx, cid);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_abo(struct rpn_ctx *ctx __unused) {
    return RPN_ERR_ILLOPC;
}

typedef enum rpn_error (*rpn_fp)(struct rpn_ctx *ctx);

static rpn_fp funcs[] = {
    rpn_op_get_reg, /* @ */
    rpn_op_add,     /* A */
    rpn_op_sub,     /* B */
    rpn_op_div,     /* C */
    rpn_op_mul,     /* D */
    rpn_op_rem,     /* E */
    rpn_op_eq,      /* F */
    rpn_op_ne,      /* G */
    rpn_op_lt,      /* H */
    rpn_op_gt,      /* I */
    rpn_op_le,      /* J */
    rpn_op_ge,      /* K */
    rpn_op_not,     /* L */
    rpn_op_and,     /* M */
    rpn_op_or,      /* N */
    rpn_op_xor,     /* O */
    rpn_op_abo,     /* P spare */
    rpn_op_abo,     /* Q spare */
    rpn_op_abo,     /* R spare */
    rpn_op_abo,     /* S spare */
    rpn_op_abo,     /* T spare */
    rpn_op_abo,     /* U spare */
    rpn_op_abo,     /* V spare */
    rpn_op_abo,     /* W spare */
    rpn_op_abo,     /* X */
    rpn_op_abo,     /* Y */
    rpn_op_abo,     /* Z */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_in,      /* a */
    rpn_op_typeof,  /* b */
    rpn_op_strcmp,  /* c */
    rpn_op_idcmp,   /* d */
    rpn_op_cidcmp,  /* e */
    rpn_op_getfld,  /* f */
};

static enum rpn_error rpn(struct rpn_ctx *ctx, char *s) {
    const char *w = " \t\n\r\f";

	for (s = strtok(s, w); s; s = strtok(0, w)) {
        size_t op = *s - '@';

        if (op < sizeof(funcs) / sizeof(void *)) {
            enum rpn_error err;
            err = funcs[op](ctx);
            if (err) {
                clear_stack(ctx);

                return err;
            }
        } else {
            struct rpn_operand *v;

            if (s[0] == '#') {
                char *e;

                v = alloc_rpn_operand(0);
                v->i = strtoull(s + 1, &e, 10);
                v->s_size = 0;
                v->s[0] = '\0';
            } else if (s[0] == '"') {
                const char *str = s + 1;
                size_t size = strlen(str) + 1;

                v = alloc_rpn_operand(size);
                v->s_size = size;
                strcpy(v->s, str);
                v->s[size - 1] = '\0';
            } else {
                clear_stack(ctx);
                return RPN_ERR_ILLOPN;
            }

            /* TODO Handle NULL */
            push(ctx, v);
        }
	}

	if (ctx->depth != 1) {
        clear_stack(ctx);
        return RPN_ERR_BADSTK;
    }

    return 0;
}

enum rpn_error rpn_bool(struct rpn_ctx *ctx, const char *s, size_t s_len, int *out) {
    char expr[s_len + 1];
    struct rpn_operand *res;
    enum rpn_error err;

    memcpy(expr, s, s_len);
    expr[s_len] = '\0';

    err = rpn(ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = to_bool(res);
    free_rpn_operand(&res);

    return 0;
}

enum rpn_error rpn_integer(struct rpn_ctx *ctx, const char *s, size_t s_len, long long *out) {
    char expr[s_len + 1];
    struct rpn_operand *res;
    enum rpn_error err;

    memcpy(expr, s, s_len);
    expr[s_len] = '\0';

    err = rpn(ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = res->i;
    free_rpn_operand(&res);

    return 0;
}
