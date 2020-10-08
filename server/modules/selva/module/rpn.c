#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cdefs.h"
#include "../rmutil/sds.h"
#include "redismodule.h"
#include "hierarchy.h"
#include "rpn.h"

#define RPN_ASSERTS         0
#define RPN_SINGLETON       0

/*
 * Codes for primitive types.
 */
#define RPN_LVTYPE_NUMBER   0 /*<! Lvalue type code for double. */
#define RPN_LVTYPE_STRING   1 /*<! Lvalue type code for string. */

/*
 * This type should match the alignment of `typedef struct redisObject` in Redis
 * so we can extract `ptr` properly. The struct is likely located in
 * `/src/server.h` in the Redis source tree.
 */
struct redisObjectAccessor {
    uint32_t _meta;
    int refcount;
    void *ptr;
};

/**
 * RedisModuleString template.
 * This string can be used to ensure that RedisModule_CreateString() will create
 * a createRawStringObject instead of using an embedded string. This must be
 * longer than OBJ_ENCODING_EMBSTR_SIZE_LIMIT and preferrably large enough to
 * fit most of the strings we'll ever see to avoid reallocs.
 */
#define RMSTRING_TEMPLATE "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#define OPERAND(ctx, x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = pop(ctx); \
    if (!x) return RPN_ERR_BADSTK

#define OPERAND_GET_S(x) \
     ((const char *)(((x)->sp) ? (x)->sp : (x)->s))

struct rpn_operand {
    struct {
        unsigned in_use :  1; /*!< In use/in stack, do not free. */
        unsigned pooled :  1; /*!< Pooled operand, do not free. */
        unsigned regist :  1; /*!< Register value, do not free. */
        unsigned spfree :  1; /*!< Free sp if set when freeing the operand. */
    } flags;
    double d;
    size_t s_size;
    struct rpn_operand *next_free; /* Next free in pool */
    const char *sp; /*!< A pointer to a user provided string */
    char s[RPN_SMALL_OPERAND_SIZE];
};

static struct rpn_operand *small_operand_pool_next;
static struct rpn_operand small_operand_pool[RPN_SMALL_OPERAND_POOL_SIZE];

const char *rpn_str_error[] = {
    "No error",
    "Out of memory",
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

static void free_rpn_operand(void *p);

__constructor static void init_pool(void) {
    struct rpn_operand *prev = NULL;

    small_operand_pool_next = &small_operand_pool[0];

    for (int i = RPN_SMALL_OPERAND_POOL_SIZE - 1; i >= 0; i--) {
        small_operand_pool[i].next_free = prev;
        prev = &small_operand_pool[i];
    }

}

struct rpn_ctx *rpn_init(RedisModuleCtx *redis_ctx, int nr_reg) {
#if RPN_SINGLETON
    static struct rpn_ctx * ctx;

    if (unlikely(!ctx)) {
        ctx = RedisModule_Alloc(sizeof(struct rpn_ctx));
    }
#else
    struct rpn_ctx * ctx;

    ctx = RedisModule_Alloc(sizeof(struct rpn_ctx));
#endif
    if (unlikely(!ctx)) {
        return NULL;
    }

    ctx->depth = 0;
    ctx->redis_ctx = redis_ctx;
    ctx->redis_hkey = NULL;
    ctx->rm_tmp_str = NULL;
    ctx->nr_reg = nr_reg;

    ctx->reg = RedisModule_Calloc(nr_reg, sizeof(struct rpn_operand *));
    if (unlikely(!ctx->reg)) {
        RedisModule_Free(ctx);
        return NULL;
    }

    return ctx;
}

void rpn_destroy(struct rpn_ctx *ctx) {
    if (ctx) {
        for (int i = 0; i < ctx->nr_reg; i++) {
            struct rpn_operand *v = ctx->reg[i];

            if (!v) {
                continue;
            }

            v->flags.in_use = 0;
            v->flags.regist = 0;

            free_rpn_operand(&v);
        }

        RedisModule_Free(ctx->reg);
#if RPN_SINGLETON
        memset(ctx, 0, sizeof(struct rpn_ctx));
#else
        RedisModule_Free(ctx);
#endif
    }
}

/**
 * Copy string to the rm_tmp_str.
 */
static int cpy2rm_tmp_str(struct rpn_ctx *ctx, const char *str, size_t len) {
    if (unlikely(!ctx->rm_tmp_str)) {
        RedisModuleString *rms;

        rms = RedisModule_CreateString(NULL, RMSTRING_TEMPLATE, sizeof(RMSTRING_TEMPLATE));
        if (unlikely(!rms)) {
            return RPN_ERR_ENOMEM;
        }

        ctx->rm_tmp_str = rms;
    }
    /*
     * The sds string pointer might change so we need to update the
     * redis object every time. This is not how the RM API nor robj
     * was meant to be used but * we know enough about the internals
     * to be confident with this.
     *
     * There is a huge performance benefit in doing all this mangling
     * as we avoid doing dozens of mallocs and frees.
     */
    sds old = (sds)RedisModule_StringPtrLen(ctx->rm_tmp_str, NULL);
    sds new = sdscpylen(old, str, len);
    ((struct redisObjectAccessor *)ctx->rm_tmp_str)->ptr = new;


    if (unlikely(!new)) {
        /* FIXME what to do with the robj? */
        ctx->rm_tmp_str = NULL;

        return RPN_ERR_ENOMEM;
    }

    return 0;
}

static struct rpn_operand *alloc_rpn_operand(size_t slen) {
    struct rpn_operand *v;

    if (slen <= RPN_SMALL_OPERAND_SIZE && small_operand_pool_next) {
        v = small_operand_pool_next;
        small_operand_pool_next = v->next_free;

        memset(v, 0, sizeof(struct rpn_operand));
        v->flags.pooled = 1;
    } else {
        const size_t size = sizeof(struct rpn_operand) + (slen - SELVA_NODE_ID_SIZE);

#if RPN_ASSERTS
        assert(size > slen);
#endif
        v = RedisModule_Calloc(1, size);
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
    if (!v || v->flags.in_use || v->flags.regist) {
        return;
    }

    if (v->flags.spfree && v->sp) {
        RedisModule_Free((void *)v->sp);
        v->sp = NULL;
    }
    if (v->flags.pooled) {
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

static double nan_undefined() {
    return nan("1");
}

static int isnan_undefined(double x) {
    long long i;

    if (!isnan(x)) {
      return 0;
    }

    memcpy(&i, &x, sizeof(i));

    return i & 1;
}

static enum rpn_error push(struct rpn_ctx *ctx, struct rpn_operand *v) {
	if (unlikely(ctx->depth >= RPN_MAX_D)) {
        fprintf(stderr, "RPN: Stack overflow\n");
        return RPN_ERR_BADSTK;
    }

    v->flags.in_use = 1;
	ctx->stack[ctx->depth++] = v;

    return RPN_ERR_OK;
}

static enum rpn_error push_double_result(struct rpn_ctx *ctx, double x) {
    struct rpn_operand *v = alloc_rpn_operand(0);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->d = x;
    push(ctx, v);

    return RPN_ERR_OK;
}

static enum rpn_error push_int_result(struct rpn_ctx *ctx, long long v) {
    return push_double_result(ctx, (double)v);
}

static enum rpn_error push_string_result(struct rpn_ctx *ctx, const char *s, size_t slen) {
    const size_t size = slen + 1;
    struct rpn_operand *v = alloc_rpn_operand(size);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->s_size = size;
    strncpy(v->s, s, slen);
    v->s[slen] = '\0';
    v->d = nan_undefined();
    push(ctx, v);

    return RPN_ERR_OK;
}

static enum rpn_error push_empty_value(struct rpn_ctx *ctx) {
    const size_t size = 2;
    struct rpn_operand *v = alloc_rpn_operand(size);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->d = 0.0;
    v->s_size = size;
    v->s[0] = '\0';
    v->s[1] = '\0';
    push(ctx, v);

    return RPN_ERR_OK;
}

/**
 * Push a RedisModuleString to the stack.
 * Note that the string must not be freed while it's still in use by rpn.
 */
static enum rpn_error push_rm_string_result(struct rpn_ctx *ctx, const RedisModuleString *rms) {
    size_t slen;
    struct rpn_operand *v = alloc_rpn_operand(0);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->sp = RedisModule_StringPtrLen(rms, &slen);
    v->s_size = slen + 1;

    push(ctx, v);

    return RPN_ERR_OK;
}

static struct rpn_operand *pop(struct rpn_ctx *ctx) {
	if (!ctx->depth) {
        return NULL;
    }

    struct rpn_operand *v = ctx->stack[--ctx->depth];

#if RPN_ASSERTS
    assert(v);
#endif

    v->flags.in_use = 0;

	return v;
}

static void clear_stack(struct rpn_ctx *ctx) {
    struct rpn_operand *v;

    while ((v = pop(ctx))) {
        v->flags.in_use = 0;
        free_rpn_operand(&v);
    }
}

static int to_bool(struct rpn_operand *v) {
    const double d = v->d;

    if (isnan(d)) {
        return v->s_size > 0 && OPERAND_GET_S(v)[0] != '\0';
    }

    return !!((long long)d);
}

enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s, size_t slen, unsigned flags) {
    struct rpn_operand *old;

    if (i >= (size_t)ctx->nr_reg) {
        return RPN_ERR_BNDS;
    }

    old = ctx->reg[i];
    if (old) {
        /* Can be freed again unless some other flag inhibits it. */
        old->flags.regist = 0;

        free_rpn_operand(&old);
    }

    if (s) {
        struct rpn_operand *r;

        r = alloc_rpn_operand(0);

        /*
         * Set the string value.
         */
        r->flags.regist = 1; /* Can't be freed when this flag is set. */
        r->flags.spfree = (flags & RPN_SET_REG_FLAG_RMFREE) == RPN_SET_REG_FLAG_RMFREE;
        r->s_size = slen;
        r->sp = s;

        /*
         * Set the integer value.
         */
        char *e = (char *)s;
        if (slen > 0) {
            r->d = strtod(s, &e);
        }
        if (e == s) {
            r->d = nan("");
        }

        ctx->reg[i] = r;
    } else { /* Otherwise just clear the register. */
        ctx->reg[i] = NULL;
    }

    return RPN_ERR_OK;
}

/*
 * This is way faster than strtoll() in glibc.
 */
static int fast_atou(const char * str)
{
    int n = 0;

    while (*str >= '0' && *str <= '9') {
        n = n * 10 + (int)(*str++) - '0';
    }

    return n;
}

static enum rpn_error rpn_get_reg(struct rpn_ctx *ctx, const char *str_index, int type) {
    char *end = NULL;
    const size_t i = fast_atou(str_index);

    if (i >= (size_t)ctx->nr_reg) {
        fprintf(stderr, "RPN: Register index out of bounds: %zu\n", i);
        return RPN_ERR_BNDS;
    }

    struct rpn_operand *r = ctx->reg[i];

    if (!r) {
        fprintf(stderr, "RPN: Register value is a NULL pointer: %zu\n", i);
        return RPN_ERR_NPE;
    }

    if (type == RPN_LVTYPE_NUMBER) {
        if (isnan(r->d)) {
            fprintf(stderr, "RPN: Register value is not a number: %zu\n", i);
            return RPN_ERR_NAN;
        }

        push(ctx, r);
    } else if (type == RPN_LVTYPE_STRING) {
        push(ctx, r);
    } else {
        fprintf(stderr, "RPN: Unknown type code: %d\n", type);
        return RPN_ERR_TYPE;
    }

    return RPN_ERR_OK;
}

static RedisModuleKey *open_hkey(struct rpn_ctx *ctx) {
    RedisModuleKey *id_key;

    if (ctx->redis_hkey) {
        id_key = ctx->redis_hkey;
    } else {
#if RPN_ASSERTS
        assert(ctx->reg[0]);
        assert(ctx->redis_ctx);
#endif

        /* Current node_id is stored in reg[0] */
        const char *id_str = OPERAND_GET_S(ctx->reg[0]);
        cpy2rm_tmp_str(ctx, id_str, strnlen(id_str, SELVA_NODE_ID_SIZE));
        RedisModuleString *id = ctx->rm_tmp_str;
        if (!id) {
            return NULL;
        }
        id_key = RedisModule_OpenKey(ctx->redis_ctx, id, REDISMODULE_READ);
        if (!id_key) {
            return NULL;
        }

        ctx->redis_hkey = id_key;
    }

    return id_key;
}

static enum rpn_error rpn_getfld(struct rpn_ctx *ctx, struct rpn_operand *field, int type) {
    RedisModuleKey *id_key;
    RedisModuleString *value = NULL;
    int err;

    id_key = open_hkey(ctx);
    if (!id_key) {
        fprintf(stderr, "RPN: Node hash not found for: \"%.*s\"\n",
                (int)SELVA_NODE_ID_SIZE, OPERAND_GET_S(ctx->reg[0]));
        return push_empty_value(ctx);
    }

    /* FIXME error handling */
    const char *field_str = OPERAND_GET_S(field);
    cpy2rm_tmp_str(ctx, field_str, strlen(field_str));
    assert(ctx->rm_tmp_str);

    err = RedisModule_HashGet(id_key, 0, ctx->rm_tmp_str, &value, NULL);
    if (err == REDISMODULE_ERR || !value) {
#if 0
        fprintf(stderr, "RPN: Field \"%s\" not found for node: \"%.*s\"\n",
                OPERAND_GET_S(field),
                (int)SELVA_NODE_ID_SIZE, (const void *)OPERAND_GET_S(ctx->reg[0]));
#endif
        return push_empty_value(ctx);
    }

    if (type == RPN_LVTYPE_NUMBER) {
        double dvalue;

        err = RedisModule_StringToDouble(value, &dvalue);
        RedisModule_FreeString(ctx->redis_ctx, value);

        if (unlikely(err != REDISMODULE_OK)) {
            fprintf(stderr, "RPN: Field value [%.*s].%.*s is not a number\n",
                    (int)SELVA_NODE_ID_SIZE, OPERAND_GET_S(ctx->reg[0]),
                    (int)field->s_size, OPERAND_GET_S(field));

            return RPN_ERR_NAN;
        }

        return push_double_result(ctx, dvalue);
    } else {
        /*
         * Supposedly there is no need to free `value`
         * because we are using automatic memory management.
         */
        return push_rm_string_result(ctx, value);
    }
}

static enum rpn_error rpn_op_add(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d + b->d);
}

static enum rpn_error rpn_op_sub(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d - b->d);
}

static enum rpn_error rpn_op_div(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    double d = b->d;

    return push_double_result(ctx, a->d / d);
}

static enum rpn_error rpn_op_mul(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d * b->d);
}

static enum rpn_error rpn_op_rem(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    long long d = (long long)b->d;

    if (d == 0) {
        return RPN_ERR_DIV;
    }

    return push_int_result(ctx, (long long)a->d % d);
}

static enum rpn_error rpn_op_eq(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d == b->d);
}

static enum rpn_error rpn_op_ne(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d != b->d);
}

static enum rpn_error rpn_op_lt(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d < b->d);
}

static enum rpn_error rpn_op_gt(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d > b->d);
}

static enum rpn_error rpn_op_le(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d <= b->d);
}

static enum rpn_error rpn_op_ge(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d >= b->d);
}

static enum rpn_error rpn_op_not(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    return push_int_result(ctx, !to_bool(a));
}

static enum rpn_error rpn_op_and(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) && to_bool(b));
}

static enum rpn_error rpn_op_or(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) || to_bool(b));
}

static enum rpn_error rpn_op_xor(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) ^ to_bool(b));
}

static enum rpn_error rpn_op_necess(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    if (!to_bool(a)) {
        return RPN_ERR_NECESS;
    }

    return push_double_result(ctx, 1.0);
}

static enum rpn_error rpn_op_exists(struct rpn_ctx *ctx) {
    int err, exists;
    RedisModuleKey *id_key;
    OPERAND(ctx, field);

    id_key = open_hkey(ctx);
    if (!id_key) {
        return push_double_result(ctx, 0.0);
    }

    err = RedisModule_HashGet(id_key, REDISMODULE_HASH_CFIELDS | REDISMODULE_HASH_EXISTS, OPERAND_GET_S(field), &exists, NULL);

    return push_int_result(ctx, err == REDISMODULE_OK && exists);
}

static enum rpn_error rpn_op_range(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    OPERAND(ctx, c);

    return push_int_result(ctx, a->d <= b->d && b->d <= c->d);
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
    const char *s = OPERAND_GET_S(a);

    if (a->s_size < SELVA_NODE_ID_SIZE) {
        return RPN_ERR_TYPE;
    }

#if SELVA_NODE_TYPE_SIZE != 2
#error Expected SELVA_NODE_TYPE_SIZE to be 2
#endif
    t[0] = s[0];
    t[1] = s[1];

    return push_string_result(ctx, t, sizeof(t));
}

static inline size_t size_min(size_t a, size_t b) {
    return (a < b ? a : b);
}

static enum rpn_error rpn_op_strcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    const size_t a_size = a->s_size;
    const size_t b_size = b->s_size;
    const ssize_t sizeDiff = b_size - a_size;

    return push_int_result(ctx, !sizeDiff &&
                           !strncmp(OPERAND_GET_S(a),
                                    OPERAND_GET_S(b),
                                    size_min(a_size, b_size)));

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_idcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    const int size_ok = a->s_size >= SELVA_NODE_ID_SIZE && b->s_size >= SELVA_NODE_ID_SIZE;

    return push_int_result(ctx, size_ok &&
                           !memcmp(OPERAND_GET_S(a), OPERAND_GET_S(b), SELVA_NODE_ID_SIZE));
}

static enum rpn_error rpn_op_cidcmp(struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

#if RPN_ASSERTS
    assert(ctx->reg[0]);
    assert(ctx->reg[0]->s_size == SELVA_NODE_ID_SIZE);
#endif

    /*
     * Note the the allocated string is always large so that the comparison is safe.
     */

    return push_int_result(ctx, !memcmp(OPERAND_GET_S(a), OPERAND_GET_S(ctx->reg[0]), SELVA_NODE_TYPE_SIZE));
}

static enum rpn_error rpn_op_getsfld(struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(ctx, field, RPN_LVTYPE_STRING);
}

static enum rpn_error rpn_op_getdfld(struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(ctx, field, RPN_LVTYPE_NUMBER);
}

static enum rpn_error rpn_op_abo(struct rpn_ctx *ctx __unused) {
    return RPN_ERR_ILLOPC;
}

typedef enum rpn_error (*rpn_fp)(struct rpn_ctx *ctx);

static rpn_fp funcs[] = {
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
    rpn_op_necess,  /* P */
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
    rpn_op_getsfld, /* f */
    rpn_op_getdfld, /* g */
    rpn_op_exists,  /* h */
    rpn_op_range,   /* i */
    rpn_op_abo,     /* j spare */
    rpn_op_abo,     /* k spare */
    rpn_op_abo,     /* l spare */
    rpn_op_abo,     /* m spare */
    rpn_op_abo,     /* n spare */
    rpn_op_abo,     /* o spare */
    rpn_op_abo,     /* p spare */
    rpn_op_abo,     /* q spare */
    rpn_op_abo,     /* r spare */
    rpn_op_abo,     /* s spare */
    rpn_op_abo,     /* t spare */
    rpn_op_abo,     /* u spare */
    rpn_op_abo,     /* v spare */
    rpn_op_abo,     /* w spare */
    rpn_op_abo,     /* x spare */
    rpn_op_abo,     /* y spare */
    rpn_op_abo,     /* z spare */
};

rpn_token *rpn_compile(const char *input, size_t len) {
    const char *w = " \t\n\r\f";
    rpn_token *expr;
    char sa[len + 1];


    memcpy(sa, input, len);
    sa[len] = '\0';

    size_t i = 0;
    size_t size = 2 * sizeof(rpn_token);

    expr = RedisModule_Alloc(size);
    if (!expr) {
        return NULL;
    }

    char *s = sa;
    for (s = strtok(s, w); s; s = strtok(0, w)) {
        rpn_token *new = RedisModule_Realloc(expr, size);
        if (new) {
            expr = new;
        } else {
            RedisModule_Free(expr);
            return NULL;
        }

        /* TODO verify s len */
        strncpy(expr[i++], s, RPN_MAX_TOKEN_SIZE - 1);
        size += sizeof(rpn_token);
    }
    memset(expr[i], 0, sizeof(rpn_token));

    return expr;
}

static enum rpn_error rpn(struct rpn_ctx *ctx, const rpn_token *expr) {
    const rpn_token *it = expr;
    const char *s;

    while (*(s = *it++)) {
        size_t op = *s - 'A';

        if (op < sizeof(funcs) / sizeof(void *)) { /* Operator */
            enum rpn_error err;
            err = funcs[op](ctx);
            if (err) {
                clear_stack(ctx);

                if (err == RPN_ERR_NECESS) {
                    /*
                     * A necessarily truthy condition failed. This can be
                     * interpreted as a short-circuited false result for the
                     * expression.
                     */
                    (void)push_double_result(ctx, 0.0);
                    break;
                }

                return err;
            }
        } else { /* Operand */
            switch (s[0]) {
            case '@':
                {
                    const char *str = s + 1;
                    enum rpn_error err;

                    err = rpn_get_reg(ctx, str, RPN_LVTYPE_NUMBER);
                    if (err) {
                        clear_stack(ctx);

                        return err;
                    }
                }
                break;
            case '$':
                {
                    const char *str = s + 1;
                    enum rpn_error err;

                    err = rpn_get_reg(ctx, str, RPN_LVTYPE_STRING);
                    if (err) {
                        clear_stack(ctx);

                        return err;
                    }
                }
                break;
            case '#':
                {
                    struct rpn_operand *v;
                    const char *str = s + 1;
                    char *e;
                    enum rpn_error err;

                    v = alloc_rpn_operand(0);
                    v->d = strtod(str, &e);
                    v->s_size = 0;
                    v->s[0] = '\0';

                    if (unlikely(e == str)) {
                        fprintf(stderr, "RPN: Operand is not a number: %s\n", s);
                        return RPN_ERR_NAN;
                    }

                    err = push(ctx, v);
                    if (err) {
                        clear_stack(ctx);
                        return err;
                    }
                }
                break;
            case '"':
                {
                    struct rpn_operand *v;
                    const char *str = s + 1;
                    size_t size = strlen(str) + 1;
                    enum rpn_error err;

#ifdef RPN_ASSERTS
                    /* We don't expect to see extra long strings here. */
                    assert(size <= 120);
#endif

                    v = alloc_rpn_operand(size);
                    v->s_size = size;
                    strncpy(v->s, str, size);
                    v->s[size - 1] = '\0';
                    v->d = nan("");

                    err = push(ctx, v);
                    if (err) {
                        clear_stack(ctx);
                        return err;
                    }
                }
                break;
            default:
                fprintf(stderr, "RPN: Illegal operand: \"%s\"\n", s);
                clear_stack(ctx);
                return RPN_ERR_ILLOPN;
            }
        }
	}

	if (ctx->depth != 1) {
        clear_stack(ctx);
        return RPN_ERR_BADSTK;
    }

    return RPN_ERR_OK;
}

static void closeHkey(struct rpn_ctx *ctx) {
    if (ctx->redis_hkey) {
        RedisModule_CloseKey(ctx->redis_hkey);
    }
    ctx->redis_hkey = NULL;
}

enum rpn_error rpn_bool(struct rpn_ctx *ctx, const rpn_token *expr, int *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(ctx, expr);
    closeHkey(ctx);
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

enum rpn_error rpn_double(struct rpn_ctx *ctx, const rpn_token *expr, double *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(ctx, expr);
    closeHkey(ctx);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = res->d;
    free_rpn_operand(&res);

    return 0;
}

enum rpn_error rpn_integer(struct rpn_ctx *ctx, const rpn_token *expr, long long *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(ctx, expr);
    closeHkey(ctx);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = (long long)round(res->d);
    free_rpn_operand(&res);

    return 0;
}
