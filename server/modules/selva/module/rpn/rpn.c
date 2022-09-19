#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jemalloc.h"
#include "cdefs.h"
#include "selva.h"
#include "../rmutil/sds.h"
#include "redismodule.h"
#include "cstrings.h"
#include "errors.h"
#include "hierarchy.h"
#include "selva_object.h"
#include "selva_set.h"
#include "selva_set_ops.h"
#include "timestamp.h"
#include "rpn.h"

#define RPN_ASSERTS         0

/*
 * Codes for primitive types.
 */
#define RPN_LVTYPE_NUMBER   0 /*<! Lvalue type code for double. */
#define RPN_LVTYPE_STRING   1 /*<! Lvalue type code for string. */
#define RPN_LVTYPE_SET      2 /*<! Lvalue type code for set. */

/**
 * Op codes for the RPN VM.
 * Every RPN expression is compiled into these few opcodes.
 */
enum rpn_code {
    RPN_CODE_STOP = 0,
    RPN_CODE_CALL,
    RPN_CODE_GET_REG_NUMBER,
    RPN_CODE_GET_REG_STRING,
    RPN_CODE_GET_REG_SET,
    RPN_CODE_GET_LIT,
    RPN_CODE_JMP_FWD,
} __packed;

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
#define RMSTRING_TEMPLATE "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#define OPERAND(ctx, x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = pop(ctx); \
    if (!x) return RPN_ERR_BADSTK

#define OPERAND_GET_S(x) \
     ((const char *)((x)->flags.spused && ((x)->sp) ? (x)->sp : (x)->s))

#define OPERAND_GET_S_LEN(x) \
    ((x)->s_size > 0 ? (x)->s_size - 1 : 0)

#define OPERAND_GET_OBJ(x) \
    ((x)->flags.slvobj ? (x)->obj : NULL)

#define OPERAND_GET_SET(x) \
    ((x)->flags.slvset ? (x)->set : NULL)

/**
 * Use to create an empty result operand pointer.
 */
#define RESULT_OPERAND(x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = NULL

struct rpn_operand {
    struct {
        unsigned pooled : 1; /*!< Pooled operand, do not free. */
        unsigned regist : 1; /*!< Register value, do not free. */
        unsigned spused : 1; /*!< The value is a string pointed by sp. */
        unsigned spfree : 1; /*!< Free sp if set when freeing the operand. */
        unsigned slvobj : 1; /*!< SelvaObject pointer. */
        unsigned slvset : 1; /*!< Set pointer is pointing to a SelvaSet. */
        unsigned embset : 1; /*!< Embedded set is used and it must be destroyed. */
    } flags;
    uint32_t refcount; /*!< Stack reference counter adjusted on push/pop. */
    struct rpn_operand *next_free; /* Next free in pool. */
    size_t s_size;
    double d;
    union {
        const char *sp; /*!< A pointer to a user provided string. */
        struct SelvaObject *obj; /*!< A SelvaObject pointer. */
        struct {
            struct SelvaSet *set; /*! A pointer to a SelvaSet. */
            /*
             * Note that set_emb is after the pointer and it's not necessarily
             * in the allocated memory block if embset is not set.
             */
            struct SelvaSet set_emb; /*!< An embedded set. */
        };
        char s[RPN_SMALL_OPERAND_SIZE];
    };
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
    "Break",
};

static void free_rpn_operand(void *p);
static void clear_stack(struct rpn_ctx *ctx);

__constructor static void init_pool(void) {
    struct rpn_operand *prev = NULL;

    small_operand_pool_next = &small_operand_pool[0];

    for (int i = RPN_SMALL_OPERAND_POOL_SIZE - 1; i >= 0; i--) {
        small_operand_pool[i].next_free = prev;
        prev = &small_operand_pool[i];
    }
}

struct rpn_ctx *rpn_init(int nr_reg) {
    struct rpn_ctx * ctx;

    if (nr_reg < 1) {
        nr_reg = 1;
    }

    ctx = selva_calloc(1, sizeof(struct rpn_ctx) + nr_reg * sizeof(struct rpn_operand *));
    ctx->nr_reg = nr_reg;

    return ctx;
}

void rpn_destroy(struct rpn_ctx *ctx) {
    if (ctx) {
        clear_stack(ctx);
        for (int i = 0; i < ctx->nr_reg; i++) {
            struct rpn_operand *v = ctx->reg[i];

            if (!v) {
                continue;
            }

            v->refcount = 0;
            v->flags.regist = 0;

            free_rpn_operand(&v);
        }

        if (ctx->rms) {
            RedisModule_FreeString(NULL, ctx->rms);
        }
        selva_free(ctx);
    }
}

void _rpn_auto_free_ctx(void *p) {
    struct rpn_ctx *ctx = *(void **)p;

    rpn_destroy(ctx);
}

static double nan_undefined(void) {
    return nan("1");
}

__used static int isnan_undefined(double x) {
    long long i;

    if (!isnan(x)) {
      return 0;
    }

    memcpy(&i, &x, sizeof(i));

    return i & 1;
}

/**
 * Copy C string to RedisModuleString.
 * Be careful to not change something that is already referenced somewhere.
 */
static int cpy2rm_str(RedisModuleString **rms_p, const char *str, size_t len) {
    RedisModuleString *rms;

    if (!*rms_p) {
        *rms_p = RedisModule_CreateString(NULL, RMSTRING_TEMPLATE, sizeof(RMSTRING_TEMPLATE));
    }

    rms = *rms_p;
    if (((struct redisObjectAccessor *)rms)->refcount > 1) {
        SELVA_LOG(SELVA_LOGL_ERR,
                  "The given RMS (%p) is already in use and cannot be modified",
                  rms);
        return RPN_ERR_NOTSUP;
    }

    /*
     * The sds string pointer might change so we need to update the
     * redis object every time. This is not how the RM API nor robj
     * was meant to be used but we know enough about the internals
     * to be confident with this.
     *
     * There is a huge performance benefit in doing all this mangling
     * as we avoid doing dozens of mallocs and frees.
     */
    sds old = (sds)RedisModule_StringPtrLen(rms, NULL);
    sds new = sdscpylen(old, str, len);
    ((struct redisObjectAccessor *)rms)->ptr = new;

    if (unlikely(!new)) {
        /*
         * Unfortunately we'll leak the robj in this case. There is no safe way
         * to recover because we are already messing with the internals of the
         * redis object. We could try to allocate a smaller buffer at this point
         * and attempt to do a partial recovery but doing such a trickery
         * doesn't seem too wise to be implemented here, given that we are
         * already hoping to never end up inside this clause.
         */
        *rms_p = NULL;

        return RPN_ERR_ENOMEM;
    }

    return 0;
}

static int rpn_operand2rms(RedisModuleString **rms, const struct rpn_operand *o) {
    const char *str = OPERAND_GET_S(o);
    const size_t len = OPERAND_GET_S_LEN(o);

    return cpy2rm_str(rms, str, len);
}

static struct rpn_operand *alloc_rpn_operand(size_t s_size) {
    struct rpn_operand *v;

    if (s_size <= RPN_SMALL_OPERAND_SIZE && small_operand_pool_next) {
        v = small_operand_pool_next;
        small_operand_pool_next = v->next_free;

        memset(v, 0, sizeof(struct rpn_operand));
        v->flags.pooled = 1;
    } else {
        const size_t size = sizeof(struct rpn_operand) - RPN_SMALL_OPERAND_SIZE + s_size;

        v = selva_calloc(1, size);
    }

    return v;
}

static struct rpn_operand *alloc_rpn_set_operand(enum SelvaSetType type) {
    struct rpn_operand *v;

    v = alloc_rpn_operand(sizeof(struct SelvaSet *) + sizeof(struct SelvaSet));
    if (!v) {
        return NULL;
    }

    v->flags.slvset = 1;
    v->flags.embset = 1;
    v->d = nan_undefined();
    v->set = &v->set_emb;
    SelvaSet_Init(v->set, type);

    return v;
}

static void free_rpn_operand(void *p) {
    struct rpn_operand **pp = (struct rpn_operand **)p;
    struct rpn_operand *v;

    if (unlikely(!pp)) {
        return;
    }

    v = *pp;
    if (!v || v->refcount > 0 || v->flags.regist) {
        return;
    }

    if (v->flags.spused && v->flags.spfree && v->sp) {
        selva_free((void *)v->sp);
    } else if (v->flags.embset && v->flags.slvset) {
        SelvaSet_Destroy(&v->set_emb);
    }
    if (v->flags.pooled) {
        struct rpn_operand *prev = small_operand_pool_next;

        /*
         * Put a pooled operand back to the pool.
         */
        small_operand_pool_next = v;
        small_operand_pool_next->next_free = prev;
    } else {
        selva_free(v);
    }
}

static struct rpn_operand *pop(struct rpn_ctx *ctx) {
    if (!ctx->depth) {
        return NULL;
    }

    struct rpn_operand *v = ctx->stack[--ctx->depth];

#if RPN_ASSERTS
    assert(v);
#endif

    v->refcount--;

    return v;
}

static enum rpn_error push(struct rpn_ctx *ctx, struct rpn_operand *v) {
    if (unlikely(ctx->depth >= RPN_MAX_D)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Stack overflow");
        return RPN_ERR_BADSTK;
    }

    v->refcount++;
    ctx->stack[ctx->depth++] = v;

    return RPN_ERR_OK;
}

static enum rpn_error push_double_result(struct rpn_ctx *ctx, double x) {
    struct rpn_operand *v = alloc_rpn_operand(0);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->d = x;

    return push(ctx, v);
}

static enum rpn_error push_int_result(struct rpn_ctx *ctx, long long x) {
    struct rpn_operand *v = alloc_rpn_operand(0);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->d = (double)x;

    return push(ctx, v);
}

static enum rpn_error push_string_result(struct rpn_ctx *ctx, const char *s, size_t slen) {
    const size_t size = slen + 1;
    struct rpn_operand *v = alloc_rpn_operand(size);

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->s_size = size;
    memcpy(v->s, s, slen);
    v->s[slen] = '\0';
    v->d = nan_undefined();

    return push(ctx, v);
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

    return push(ctx, v);
}

/**
 * Push a RedisModuleString to the stack.
 * Note that the string must not be freed while it's still in use by rpn.
 */
static enum rpn_error push_rm_string_result(struct rpn_ctx *ctx, const RedisModuleString *rms) {
    size_t slen;
    struct rpn_operand *v = alloc_rpn_operand(sizeof(RedisModuleString *));

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->flags.spused = 1;
    v->sp = RedisModule_StringPtrLen(rms, &slen);
    v->s_size = slen + 1;
    v->d = nan_undefined();

    return push(ctx, v);
}

static enum rpn_error push_selva_set_result(struct rpn_ctx *ctx, struct SelvaSet *set) {
    struct rpn_operand *v = alloc_rpn_operand(sizeof(struct SelvaSet *));

    if (unlikely(!v)) {
        return RPN_ERR_ENOMEM;
    }

    v->flags.slvset = 1;
    v->set = set;
    v->d = nan_undefined();

    return push(ctx, v);
}

static void clear_stack(struct rpn_ctx *ctx) {
    struct rpn_operand *v;

    while ((v = pop(ctx))) {
        free_rpn_operand(&v);
    }
}

static int to_bool(struct rpn_operand *v) {
    const double d = v->d;

    if (isnan(d)) {
        if (v->flags.slvobj) {
            struct SelvaObject *obj = OPERAND_GET_OBJ(v);

            return obj && SelvaObject_LenStr(obj, NULL, 0);
        } else if (v->flags.slvset) {
            const struct SelvaSet *set = OPERAND_GET_SET(v);

            return set && set->size > 0;
        }
        return v->s_size > 0 && OPERAND_GET_S(v)[0] != '\0';
    }

    return !!((long long)d);
}

static void clear_old_reg(struct rpn_ctx *ctx, size_t i) {
    struct rpn_operand *old;

    old = ctx->reg[i];
    if (old) {
        /* Can be freed again unless some other flag inhibits it. */
        old->flags.regist = 0;

        free_rpn_operand(&old);
    }
}

enum rpn_error rpn_set_reg(struct rpn_ctx *ctx, size_t i, const char *s, size_t size, unsigned flags) {
    if (i >= (size_t)ctx->nr_reg) {
        return RPN_ERR_BNDS;
    }

    clear_old_reg(ctx, i);

    if (s) {
        struct rpn_operand *r;

        r = alloc_rpn_operand(sizeof(char *));
        if (!r) {
            return RPN_ERR_ENOMEM;
        }

        /*
         * Set the string value.
         */
        r->flags.regist = 1; /* Can't be freed when this flag is set. */
        r->flags.spused = 1;
        r->flags.spfree = (flags & RPN_SET_REG_FLAG_SELVA_FREE) == RPN_SET_REG_FLAG_SELVA_FREE;
        r->s_size = size;
        r->sp = s;

        /*
         * Set the integer value.
         */
        if (flags & RPN_SET_REG_FLAG_IS_NAN) {
            r->d = nan("");
        } else {
            char *e = (char *)s;
            if (size > 0) {
                r->d = strtod(s, &e);
            }
            if (e == s) {
                r->d = nan("");
            }
        }

        ctx->reg[i] = r;
    } else { /* Otherwise just clear the register. */
        ctx->reg[i] = NULL;
    }

    return RPN_ERR_OK;
}

enum rpn_error rpn_set_reg_rms(struct rpn_ctx *ctx, size_t i, RedisModuleString *rms) {
    TO_STR(rms);
    const size_t size = rms_len + 1;
    char *arg;

    arg = selva_malloc(size);
    memcpy(arg, rms_str, size);
    return rpn_set_reg(ctx, i, arg, size, RPN_SET_REG_FLAG_SELVA_FREE);
}

/* TODO free flag for rpn_set_reg_slvobj() */
enum rpn_error rpn_set_reg_slvobj(struct rpn_ctx *ctx, size_t i, struct SelvaObject *obj, unsigned flags __unused) {
    if (i >= (size_t)ctx->nr_reg) {
        return RPN_ERR_BNDS;
    }

    clear_old_reg(ctx, i);

    if (obj) {
        struct rpn_operand *r;

        r = alloc_rpn_operand(sizeof(struct SelvaObject *));

        /*
         * Set the values.
         */
        r->flags.regist = 1;
        r->flags.slvobj = 1;
        r->d = nan_undefined();
        r->obj = obj;

        ctx->reg[i] = r;
    } else {
        ctx->reg[i] = NULL;
    }

    return RPN_ERR_OK;
}

/* TODO free flag for rpn_set_reg_slvset() */
enum rpn_error rpn_set_reg_slvset(struct rpn_ctx *ctx, size_t i, struct SelvaSet *set, unsigned flags __unused) {
    if (i >= (size_t)ctx->nr_reg) {
        return RPN_ERR_BNDS;
    }

    clear_old_reg(ctx, i);

    if (set) {
        struct rpn_operand *r;

        /*
         * Note that the pointer exists in the struct before the embedded
         * SelvaSet, and therefore we just don't allocate memory for it
         * because it isn't needed.
         */
        r = alloc_rpn_operand(sizeof(struct SelvaSet *));

        /*
         * Set the values.
         */
        r->flags.regist = 1;
        r->flags.slvset = 1;
        r->d = nan_undefined();
        r->set = set;

        ctx->reg[i] = r;
    } else {
        ctx->reg[i] = NULL;
    }

    return RPN_ERR_OK;
}

static enum rpn_error SelvaSet_Add_err2rpn_error(int err) {
    if (err && err != SELVA_EEXIST) {
        if (err == SELVA_ENOMEM) {
            return RPN_ERR_ENOMEM;
        } else {
            /* Report other errors as a type error. */
            return RPN_ERR_TYPE;
        }
    }

    return RPN_ERR_OK;
}

static enum rpn_error SelvaSet_Union_err2rpn_error(int err) {
    if (err >= 0) {
        return RPN_ERR_OK;
    } else if (err == SELVA_ENOMEM) {
        return RPN_ERR_ENOMEM;
    } else if (err == SELVA_EINVAL) {
        return RPN_ERR_ILLOPN;
    } else if (err == SELVA_EINTYPE) {
        return RPN_ERR_TYPE;
    } else {
        /* Report other errors as type errors. */
        return RPN_ERR_TYPE;
    }
}

static enum rpn_error add2slvset_res(struct rpn_operand *res, const char *str, size_t len) {
    RedisModuleString *s;
    int err;

    s = RedisModule_CreateString(NULL, str, len);
    err = SelvaSet_Add(res->set, s);
    if (err) {
        RedisModule_FreeString(NULL, s);

        if (err != SELVA_EEXIST) {
            return SelvaSet_Add_err2rpn_error(err);
        }
    }

    return RPN_ERR_OK;
}

static enum rpn_error add_rec_key2slvset_res(
        struct rpn_operand *res,
        const char *field_str, size_t field_len,
        const char *key_str, size_t key_len) {
    size_t full_field_len = field_len + key_len + 1;
    char full_field_str[full_field_len + 1];

    snprintf(full_field_str, full_field_len + 1, "%.*s.%.*s",
            (int)field_len, field_str,
            (int)key_len, key_str);
    return add2slvset_res(res, full_field_str, full_field_len);
}

/*
 * This is way faster than strtoll() in glibc.
 */
static int fast_atou(const char * str) {
    int n = 0;

    while (*str >= '0' && *str <= '9') {
        n = n * 10 + (int)(*str++) - '0';
    }

    return n;
}

static inline double js_fmod(double x, double y) {
    double result = remainder(fabs(x), (y = fabs(y)));
    if (signbit(result)) {
        result += y;
    }

    return copysign(result, x);
}

static enum rpn_error rpn_getfld(struct rpn_ctx *ctx, const struct rpn_operand *field, int type) {
    const char *field_str = OPERAND_GET_S(field);
    const size_t field_len = OPERAND_GET_S_LEN(field);
    struct SelvaObjectAny any;
    int err;

    if (!ctx->obj) {
        return RPN_ERR_NPE;
    }

    err = SelvaObject_GetAnyStr(ctx->obj, field_str, field_len, &any);
    if (err || any.type == SELVA_OBJECT_NULL) {
        if (err == SELVA_ENOENT) {
            return (type == RPN_LVTYPE_NUMBER) ? push_double_result(ctx, nan_undefined()) : push_empty_value(ctx);
        }
        return RPN_ERR_ENOMEM; /* Presumably this the only other relevant error. */
    }

    const enum SelvaObjectType field_type = any.type;
    if (field_type == SELVA_OBJECT_SET) {
        if (likely(any.set)) {
            /*
             * We don't need to care about the type of the set yet because all the
             * future operations are typesafe anyway.
             */
            return push_selva_set_result(ctx, any.set);
        } else {
            /* RFE Should we return nan_undefined() for RPN_LVTYPE_NUMBER */
            return push_empty_value(ctx);
        }
    } else { /* Primitive type */
        if (type == RPN_LVTYPE_NUMBER) {
            if (field_type == SELVA_OBJECT_DOUBLE) {
                return push_double_result(ctx, any.d);
            } else if (field_type == SELVA_OBJECT_LONGLONG) {
                return push_double_result(ctx, (double)any.ll);
            } else {
                const char *type_str = SelvaObject_Type2String(field_type, NULL);

                SELVA_LOG(SELVA_LOGL_ERR, "Field value [%.*s].%.*s is not a number, actual type: \"%s\"",
                          (int)SELVA_NODE_ID_SIZE, OPERAND_GET_S(ctx->reg[0]),
                          (int)field->s_size, OPERAND_GET_S(field),
                          type_str ? type_str : "INVALID");

                return RPN_ERR_NAN;
            }
        } else { /* Assume RPN_LVTYPE_STRING */
            if (field_type == SELVA_OBJECT_STRING && any.str) {
                return push_rm_string_result(ctx, any.str);
            } else {
#if 0
                fprintf(stderr, "%s:%d: Field \"%s\" not found in node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        OPERAND_GET_S(field),
                        (int)SELVA_NODE_ID_SIZE, (const void *)OPERAND_GET_S(ctx->reg[0]));
#endif
                return push_empty_value(ctx);
            }
        }
    }
}

static enum rpn_error rpn_op_add(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d + b->d);
}

static enum rpn_error rpn_op_sub(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d - b->d);
}

static enum rpn_error rpn_op_div(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d / b->d);
}

static enum rpn_error rpn_op_mul(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d * b->d);
}

static enum rpn_error rpn_op_rem(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, js_fmod(a->d, b->d));
}

static enum rpn_error rpn_op_eq(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d == b->d);
}

static enum rpn_error rpn_op_ne(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d != b->d);
}

static enum rpn_error rpn_op_lt(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d < b->d);
}

static enum rpn_error rpn_op_gt(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d > b->d);
}

static enum rpn_error rpn_op_le(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d <= b->d);
}

static enum rpn_error rpn_op_ge(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d >= b->d);
}

static enum rpn_error rpn_op_not(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    return push_int_result(ctx, !to_bool(a));
}

static enum rpn_error rpn_op_and(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) && to_bool(b));
}

static enum rpn_error rpn_op_or(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) || to_bool(b));
}

static enum rpn_error rpn_op_xor(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, to_bool(a) ^ to_bool(b));
}

static enum rpn_error rpn_op_necess(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    if (to_bool(a)) {
        return push(ctx, a); /* Push back. */
    } else {
        enum rpn_error err;

        err = push_double_result(ctx, 0.0);

        return err ? err : RPN_ERR_BREAK;
    }
}

static enum rpn_error rpn_op_possib(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    if (to_bool(a)) {
        enum rpn_error err;

        err = push(ctx, a);

        return err ? err : RPN_ERR_BREAK;
    } else {
        return push(ctx, a); /* Push back. */
    }
}

static enum rpn_error rpn_op_dup(struct RedisModuleCtx *redis_crx __unused, struct rpn_ctx *ctx) {
    enum rpn_error err;
    OPERAND(ctx, a);

    err = push(ctx, a);
    return err ? err : push(ctx, a);
}

static enum rpn_error rpn_op_swap(struct RedisModuleCtx *redis_crx __unused, struct rpn_ctx *ctx) {
    enum rpn_error err;
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    err = push(ctx, a);
    return err ? err : push(ctx, b);
}

static enum rpn_error rpn_op_ternary(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    OPERAND(ctx, c);

    if (to_bool(a)) {
        return push(ctx, b);
    } else {
        return push(ctx, c);
    }
}

static enum rpn_error rpn_op_drop(struct RedisModuleCtx *redis_crx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_rot(struct RedisModuleCtx *redis_crx __unused, struct rpn_ctx *ctx) {
    enum rpn_error err;
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    OPERAND(ctx, c);

    err = push(ctx, b);
    if (err) {
        return err;
    }
    err = push(ctx, c);
    if (err) {
        return err;
    }
    return push(ctx, a);
}

static enum rpn_error rpn_op_over(struct RedisModuleCtx *redis_crx __unused, struct rpn_ctx *ctx) {
    enum rpn_error err;
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    err = push(ctx, b);
    if (err) {
        return err;
    }
    err = push(ctx, a);
    if (err) {
        return err;
    }
    return push(ctx, b);
}

static enum rpn_error rpn_op_exists(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    int exists;
    OPERAND(ctx, field);
    const char *field_str = OPERAND_GET_S(field);
    const size_t field_len = OPERAND_GET_S_LEN(field);
    const struct SelvaHierarchyNode *node = ctx->node;

    /*
     * First check if it's a non-empty hierarchy/edge field.
     */
    if (node && SelvaHierarchy_IsNonEmptyField(node, field_str, field_len) > 0) {
        return push_int_result(ctx, 1);
    }

    if (!ctx->obj) {
        return push_int_result(ctx, 0);
    }

    /*
     * Finally check if it's a node object field.
     */
    exists = !SelvaObject_ExistsStr(ctx->obj, field_str, field_len);

    return push_int_result(ctx, exists);
}

static enum rpn_error rpn_op_range(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    OPERAND(ctx, c);

    return push_int_result(ctx, a->d <= b->d && b->d <= c->d);
}

static enum rpn_error rpn_op_has(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    struct SelvaSet *set;
    OPERAND(ctx, s); /* set */
    OPERAND(ctx, v); /* value */

    set = OPERAND_GET_SET(s); /* The operand `s` is a set if this gets set. */
    if (!set) {
        /* The operand `s` is a field_name string. */
        const char *field_name_str = OPERAND_GET_S(s);
        const size_t field_name_len = OPERAND_GET_S_LEN(s);

        if (!ctx->obj) {
            return RPN_ERR_NPE;
        }

        /*
         * First check if it's a data field because we can optimize this a bit by
         * using SelvaSet_Has().
         */
        set = SelvaObject_GetSetStr(ctx->obj, field_name_str, field_name_len);
    }

    if (set) {
        if (set->type == SELVA_SET_TYPE_RMSTRING) {
            if(rpn_operand2rms(&ctx->rms, v)) {
                return RPN_ERR_ENOMEM;
            }

            return push_int_result(ctx, SelvaSet_Has(set, ctx->rms));
        } else if (set->type == SELVA_SET_TYPE_DOUBLE) {
            return push_int_result(ctx, SelvaSet_Has(set, v->d));
        } else if (set->type == SELVA_SET_TYPE_LONGLONG) {
            return push_int_result(ctx, SelvaSet_Has(set, (long long)v->d));
        } else {
            return push_int_result(ctx, 0);
        }
    } else {
        /*
         * Perhaps it's a set-like field.
         */
        const char *field_name_str = OPERAND_GET_S(s);
        const size_t field_name_len = OPERAND_GET_S_LEN(s);
        int res;

        if (OPERAND_GET_SET(v) || OPERAND_GET_OBJ(v)) {
            return RPN_ERR_TYPE;
        } else if (isnan(v->d) || v->s_size > 0) { /* Assume string */
            const char *value_str = OPERAND_GET_S(v);
            const size_t value_len = OPERAND_GET_S_LEN(v);

            res = SelvaSet_field_has_string(redis_ctx,ctx->hierarchy, ctx->node, field_name_str, field_name_len, value_str, value_len);
        } else { /* Assume number */
            res = SelvaSet_field_has_double(redis_ctx, ctx->hierarchy, ctx->node, field_name_str, field_name_len, v->d);
        }

        return push_int_result(ctx, !!res);
    }
}

static enum rpn_error rpn_op_typeof(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    char t[SELVA_NODE_TYPE_SIZE];
    const char *s = OPERAND_GET_S(a);

    if (a->s_size < SELVA_NODE_ID_SIZE) {
        return RPN_ERR_TYPE;
    }

    memcpy(t, s, SELVA_NODE_TYPE_SIZE);

    return push_string_result(ctx, t, sizeof(t));
}

static enum rpn_error rpn_op_strcmp(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    const size_t a_size = a->s_size;
    const size_t b_size = b->s_size;
    const ssize_t sizeDiff = b_size - a_size;

    return push_int_result(ctx, !sizeDiff &&
                           !strncmp(OPERAND_GET_S(a),
                                    OPERAND_GET_S(b),
                                    a_size));
}

static enum rpn_error rpn_op_idcmp(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    const int size_ok = a->s_size >= SELVA_NODE_ID_SIZE && b->s_size >= SELVA_NODE_ID_SIZE;

    return push_int_result(ctx, size_ok &&
                           !memcmp(OPERAND_GET_S(a), OPERAND_GET_S(b), SELVA_NODE_ID_SIZE));
}

static enum rpn_error rpn_op_cidcmp(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);

#if RPN_ASSERTS
    assert(ctx->reg[0]);
    assert(ctx->reg[0]->s_size == SELVA_NODE_ID_SIZE);
#endif

    /*
     * Note the the allocated string is always large enough,
     * so the comparison is safe.
     */
    return push_int_result(ctx, !Selva_CmpNodeType(OPERAND_GET_S(a), OPERAND_GET_S(ctx->reg[0])));
}

static enum rpn_error rpn_op_getsfld(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(ctx, field, RPN_LVTYPE_STRING);
}

static enum rpn_error rpn_op_getdfld(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(ctx, field, RPN_LVTYPE_NUMBER);
}

static enum rpn_error rpn_op_ffirst(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    struct SelvaSet *set_a;
    RESULT_OPERAND(result);
    struct SelvaSetElement *el;
    const struct SelvaHierarchyNode *node = ctx->node;

    if (!node) {
        return RPN_ERR_ILLOPN;
    }

    set_a = OPERAND_GET_SET(a);
    if (!set_a || set_a->type != SELVA_SET_TYPE_RMSTRING) {
        return RPN_ERR_TYPE;
    }

    result = alloc_rpn_set_operand(SELVA_SET_TYPE_RMSTRING);
    if (!result) {
        return RPN_ERR_ENOMEM;
    }

    SELVA_SET_RMS_FOREACH(el, set_a) {
        size_t field_len;
        const char *field_str = RedisModule_StringPtrLen(el->value_rms, &field_len);

        if (SelvaHierarchy_IsNonEmptyField(node, field_str, field_len)) {
            enum rpn_error err;

            /*
             * If we'd be care careful we could potentially reuse the original
             * string, but let's be on the safe side because you never know how
             * the original string was created.
             */
            err = add2slvset_res(result, field_str, field_len);
            if (err) {
                return err;
            }

            break;
        }
    }

    push(ctx, result);
    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_aon(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    struct SelvaSet *set_a;
    struct SelvaSetElement *el;
    const struct SelvaHierarchyNode *node = ctx->node;

    if (!node) {
        return RPN_ERR_ILLOPN;
    }

    set_a = OPERAND_GET_SET(a);
    if (!set_a || set_a->type != SELVA_SET_TYPE_RMSTRING) {
        return RPN_ERR_TYPE;
    }

    SELVA_SET_RMS_FOREACH(el, set_a) {
        size_t field_len;
        const char *field_str = RedisModule_StringPtrLen(el->value_rms, &field_len);

        if (!SelvaHierarchy_IsNonEmptyField(node, field_str, field_len)) {
            RESULT_OPERAND(result);

            result = alloc_rpn_set_operand(SELVA_SET_TYPE_RMSTRING);
            if (!result) {
                return RPN_ERR_ENOMEM;
            }

            /* Push empty set as a result. */
            push(ctx, result);
            return RPN_ERR_OK;
        }
    }

    /* Push the original set as a result. */
    push(ctx, a);
    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_in(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    struct SelvaSet *set_a;
    struct SelvaSet *set_b;
    OPERAND(ctx, a); /* set A */
    OPERAND(ctx, b); /* set B */
    int res = 0;

    set_a = OPERAND_GET_SET(a);
    set_b = OPERAND_GET_SET(b);

    if (set_a && set_b) {
        res = SelvaSet_seta_in_setb(set_a, set_b);
    } else if (set_a && !set_b) {
        const char *field_str = OPERAND_GET_S(b);
        const size_t field_len = OPERAND_GET_S_LEN(b);

        res = SelvaSet_seta_in_fieldb(redis_ctx, set_a, ctx->hierarchy, ctx->node, field_str, field_len);
    } else if (!set_a && set_b) {
        const char *field_str = OPERAND_GET_S(a);
        const size_t field_len = OPERAND_GET_S_LEN(a);

        res = SelvaSet_fielda_in_setb(redis_ctx, ctx->hierarchy, ctx->node, field_str, field_len, set_b);
    } else if (!set_a && !set_b) {
        const char *field_a_str = OPERAND_GET_S(a);
        const size_t field_a_len = OPERAND_GET_S_LEN(a);
        const char *field_b_str = OPERAND_GET_S(b);
        const size_t field_b_len = OPERAND_GET_S_LEN(b);

        res = SelvaSet_fielda_in_fieldb(redis_ctx, ctx->hierarchy, ctx->node, field_a_str, field_a_len, field_b_str, field_b_len);
    }

    return push_int_result(ctx, res);
}

static enum rpn_error rpn_op_str_includes(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_int_result(ctx, !!memmem(OPERAND_GET_S(a), OPERAND_GET_S_LEN(a), OPERAND_GET_S(b), OPERAND_GET_S_LEN(b)));
}

static enum rpn_error rpn_op_get_clock_realtime(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    return push_int_result(ctx, ts_now());
}

static enum rpn_error rpn_op_rec_filter(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a); /* field */
    OPERAND(ctx, b); /* operator */
    OPERAND(ctx, c); /* value */
    RESULT_OPERAND(res);
    const char *field_str = OPERAND_GET_S(a);
    const size_t field_len = OPERAND_GET_S_LEN(a);
    const char sel = OPERAND_GET_S(b)[0];
    const char op = OPERAND_GET_S(b)[1]; /* This should be always valid. */
    const char *v_str = OPERAND_GET_S(c);
    const size_t v_len = OPERAND_GET_S_LEN(c);
    struct SelvaObject *edges;
    struct SelvaObject *obj;
    SelvaObject_Iterator *it;
    const char *tmp;
    const char *last_rec_key_str = NULL; /* Last match for 'l' */
    size_t last_rec_key_len = 0; /* Last match for 'l' */
    int err;

    if (OPERAND_GET_S_LEN(b) != 2) {
        return RPN_ERR_ILLOPC;
    }

    /*
     * a = all
     * f = first match
     * l = last match
     */
    if (!strpbrk((char []){ sel, '\0' }, "afl")) {
        return RPN_ERR_ILLOPN;
    }

    if (v_len == 0) {
        return RPN_ERR_ILLOPN;
    }

    if (!ctx->obj || !ctx->node) {
        return RPN_ERR_NPE;
    }

    /* RFE Is it possible to know if this is a record? */
    edges = SelvaHierarchy_GetNodeMetadataByPtr(ctx->node)->edge_fields.edges;
    if (!edges) {
        return push_empty_value(ctx);
    }

    err = SelvaObject_GetObjectStr(edges, field_str, field_len, &obj);
    if (err) {
        return push_empty_value(ctx);
    }

    res = alloc_rpn_set_operand(SELVA_SET_TYPE_RMSTRING);
    if (!res) {
        return RPN_ERR_ENOMEM;
    }

    it = SelvaObject_ForeachBegin(obj);
    while ((tmp = SelvaObject_ForeachKey(obj, &it))) {
        const char *rec_key_str = tmp;
        const size_t rec_key_len = strlen(rec_key_str);
        int r = strcmp(rec_key_str, v_str);
        int match;

        switch (op) {
        case 'F': /* == */
            match = r == 0;
            break;
        case 'G': /* != */
            match = r != 0;
            break;
        case 'H': /* < */
            match = r < 0;
            break;
        case 'I': /* > */
            match = r > 0;
            break;
        case 'J': /* <= */
            match = r <= 0;
            break;
        case 'K': /* >= */
            match = r >= 0;
            break;
        case 'm': /* rec_key.includes(v) */
            match = !!memmem(rec_key_str, rec_key_len, v_str, v_len);
            break;
        default:
            return RPN_ERR_ILLOPC;
        }

        if (match) {
            if (sel == 'a' || sel == 'f') {
                enum rpn_error rpn_err;

                rpn_err = add_rec_key2slvset_res(res, field_str, field_len, rec_key_str, rec_key_len);
                if (rpn_err) {
                    return rpn_err;
                }

                /* First match breaks. */
                if (sel == 'f') {
                    break;
                }
            } else /* if (sel = 'l') */ {
                last_rec_key_str = rec_key_str;
                last_rec_key_len = rec_key_len;
            }
        }
    }

    /* Return only last match. */
    if (sel == 'l' && last_rec_key_str) {
        enum rpn_error rpn_err;

        rpn_err = add_rec_key2slvset_res(res, field_str, field_len, last_rec_key_str, last_rec_key_len);
        if (rpn_err) {
            return rpn_err;
        }
    }

    return push(ctx, res);
}

static enum rpn_error rpn_op_union(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    RESULT_OPERAND(res);
    struct SelvaSet *set_a;
    struct SelvaSet *set_b;
    int err;

    set_a = OPERAND_GET_SET(a);
    set_b = OPERAND_GET_SET(b);
    if (!(set_a && set_b) || (set_a->type != set_b->type)) {
        return RPN_ERR_TYPE;
    }

    res = alloc_rpn_set_operand(set_a->type);
    if (!res) {
        return RPN_ERR_ENOMEM;
    }

    err = SelvaSet_Union(res->set, set_a, set_b, NULL);
    if (err) {
        return SelvaSet_Union_err2rpn_error(err);
    }

    return push(ctx, res);
}

static enum rpn_error rpn_op_abo(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused) {
    return RPN_ERR_ILLOPC;
}

static enum rpn_error rpn_op_nop(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused) {
    return RPN_ERR_OK;
}

static enum rpn_error rpn_op_ret(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused) {
    return RPN_ERR_BREAK;
}

typedef enum rpn_error (*rpn_fp)(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx);

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
    rpn_op_possib,  /* Q */
    rpn_op_dup,     /* R */
    rpn_op_swap,    /* S */
    rpn_op_ternary, /* T */
    rpn_op_drop,    /* U */
    rpn_op_over,    /* V */
    rpn_op_rot,     /* W */
    rpn_op_nop,     /* X */
    rpn_op_abo,     /* Y spare */
    rpn_op_ret,     /* Z */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_abo,     /* N/A */
    rpn_op_has,     /* a */
    rpn_op_typeof,  /* b */
    rpn_op_strcmp,  /* c */
    rpn_op_idcmp,   /* d */
    rpn_op_cidcmp,  /* e */
    rpn_op_getsfld, /* f */
    rpn_op_getdfld, /* g */
    rpn_op_exists,  /* h */
    rpn_op_range,   /* i */
    rpn_op_ffirst,  /* j */
    rpn_op_aon,     /* k */
    rpn_op_in,      /* l */
    rpn_op_str_includes, /* m */
    rpn_op_get_clock_realtime, /* n */
    rpn_op_rec_filter, /* o */
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
    rpn_op_union,   /* z */
};

#define RPN_DELIM " \t\n\r\f"
#define RPN_GROUP "{\""

/**
 * Tokenize a C-string.
 * The tokenizer can be used to tokenize RPN expressions and structures within
 * the expressions, e.g. the set notation.
 */
static const char *tokenize(const char *s, const char *delim, const char *group, const char **rest, size_t *len) {
    const char *spanp;
    const char *tok;
    char c;
    char sc;

    if (!s && !(s = *rest)) {
        return NULL;
    }

    /* Skip any leading delimiters. */
cont:
    c = *s++;
    for (spanp = delim; (sc = *spanp++) != '\0';) {
        if (c == sc) {
            goto cont;
        }
    }

    if (c == '\0') {
        *rest = NULL;
        return NULL;
    }
    tok = s - 1;

    /* Look for a grouping char. */
    char groupc = '\0';
    if (group) {
        for (spanp = group; (sc = *spanp++) != '\0';) {
            if (c == sc) {
                if (sc == '{') {
                    groupc = '}';
                } else {
                    groupc = sc;
                }
                break;
            }
        }
    }

    do {
        c = *s++;

        if (groupc == '\0') {
            spanp = delim;

            do {
                if ((sc = *spanp++) == c) {
                    *len = (size_t)(s - tok - 1);
                    if (c == '\0') {
                        s = NULL;
                    }
                    *rest = s;
                    return tok;
                }
            } while (sc != '\0');
        } else if (c == groupc) {
            *rest = s;
            *len = (size_t)(s - tok);
            return tok;
        } else if (c == '\0') {
            /* Invalid quoted string. */
            *rest = NULL;
            *len = 0;
            return NULL;
        }
    } while (1);
}

/**
 * Parse a potential jump label.
 * .<number>:
 * .1:
 * @returns 0 if no label was found;
 *          -1 if the label was invald;
 *          Otherwise the label number is returned.
 */
static int compile_parse_label(const char *tok_str, size_t tok_len) {
    char tmp[11];
    int valid = 0;

    if (tok_len < 3 || tok_str[0] != '.') {
        return 0;
    }

    memset(tmp, '\0', sizeof(tmp));
    for (size_t i = 1; i < min(tok_len, sizeof(tmp) - 1); i++) {
        char c = tok_str[i];

        if (isdigit(c)) {
            tmp[i - 1] = c;
        } else if (c == ':') {
            valid = 1;
            break;
        } else {
            return -1;
        }
    }

    if (!valid) {
        return -1;
    }

    tmp[sizeof(tmp) - 1] = '\0';
    int l = fast_atou(tmp);

    if (l == 0 || l >= RPN_MAX_LABELS) {
        return -1;
    }

    return l;
}

/**
 * Skip a jump label in a token.
 * Modifies the tok_str_p to point to the character right after the label if
 * found and returns the length of the remaining string.
 * @returns tok_len - label_len
 */
static size_t compile_skip_label(const char **tok_str_p, size_t tok_len) {
    const char *tok_str = *tok_str_p;

    if (compile_parse_label(tok_str, tok_len) <= 0) {
        return tok_len;
    }

    const char *code_begin = strchr(tok_str, ':') + 1;
    ssize_t label_len = (ssize_t)(code_begin - tok_str);

    *tok_str_p = code_begin;
    return (size_t)(max((ssize_t)tok_len - label_len, 0l));
}

/**
 * Find and map all labels in the input expression.
 * @param labels is a map from label id to token index.
 * @param input is a pointer to the original RPN expression.
 */
static int compile_find_labels(int labels[RPN_MAX_LABELS], const char *input) {
    const char *delim = RPN_DELIM;
    const char *group = RPN_GROUP;
    size_t tok_len = 0;
    const char *rest = NULL;
    int i = 0;

    for (const char *tok_str = tokenize(input, delim, group, &rest, &tok_len);
         tok_str != NULL;
         tok_str = tokenize(NULL, delim, group, &rest, &tok_len)) {
        int l = compile_parse_label(tok_str, tok_len);

        if (l > 0) {
            if (labels[l] != 0) {
                /* Disallow using the same label twice. */
                return RPN_ERR_BADSTK;
            }
            labels[l] = i;
        } else if (l == -1) {
            /* Invalid label. */
            return RPN_ERR_ILLOPN;
        }
        i++;
    }

    return RPN_ERR_OK;
}

/**
 * Store a literal in the literal register file.
 * The literal register file (literal_reg) works similar to the user defined
 * register file but it only contains the literals used in the RPN expression.
 */
static enum rpn_error compile_store_literal(struct rpn_expression *expr, size_t i, struct rpn_operand *v) {
    if (i >= RPN_MAX_D - 1) {
        return RPN_ERR_BADSTK;
    }

    v->flags.regist = 1;
    expr->literal_reg[i] = v;

    return RPN_ERR_OK;
}

/**
 * Parse a numeric literal into an operand and store it in the literal register file.
 */
static enum rpn_error compile_num_literal(struct rpn_expression *expr, size_t i, const char *str, size_t len) {
    char s[len + 1];
    char *e;
    double d;
    RESULT_OPERAND(v);

    memcpy(s, str, len);
    s[len] = '\0';
    d = strtod(s, &e);

    if (unlikely(e == s)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Operand is not a number: #%s", s);
        return RPN_ERR_NAN;
    }

    v = alloc_rpn_operand(0);
    v->d = d;
    v->s_size = 0;
    v->s[0] = '\0';

    return compile_store_literal(expr, i, v);
}

/**
 * Parse a string literal into an operand and store it in the literal register file.
 */
static enum rpn_error compile_str_literal(struct rpn_expression *expr, size_t i, const char *str, size_t len) {
    size_t size = len + 1;
    RESULT_OPERAND(v);

    v = alloc_rpn_operand(size);
    v->s_size = size;
    memcpy(v->s, str, len);
    v->s[len] = '\0';
    v->d = nan("");

    return compile_store_literal(expr, i, v);
}

/**
 * Parse a set literal into a SelvaSet operand and store it in the literal register file.
 */
static enum rpn_error compile_selvaset_literal(struct rpn_expression *expr, size_t i, const char *str, size_t len) {
    const char type = *str;
    RESULT_OPERAND(v);

    if (type != '"' &&
        type != '}' /* empty set */
       ) {
        /* Currently only string sets are supported. */
        return RPN_ERR_TYPE;
    }

    v = alloc_rpn_set_operand(SELVA_SET_TYPE_RMSTRING);

    if (type != '}') {
        char tmp[len + 1];
        const char *delim = ",";
        const char *group = "\"";
        size_t tok_len = 0;
        const char *rest = NULL;

        memcpy(tmp, str, len);
        tmp[len] = '\0';

        for (const char *tok_str = tokenize(tmp, delim, group, &rest, &tok_len);
             tok_str != NULL;
             tok_str = tokenize(NULL, delim, group, &rest, &tok_len)) {
            enum rpn_error err;

            if (tok_len == 0 || tok_str[0] != '\"' || tok_str[tok_len - 1] != '\"') {
                return RPN_ERR_ILLOPN;
            }
            tok_str++;
            tok_len -= 2;

            err = add2slvset_res(v, tok_str, tok_len);
            if (err) {
                return err;
            }
        }
        if (tok_len == 0) {
            return RPN_ERR_ILLOPN;
        }
    }

    return compile_store_literal(expr, i, v);
}

/**
 * Write rpn_code following an uint32_t value to the expression token e.
 * RPN code is the final form that can be executed/evaluated by the RPN VM.
 */
static inline void compile_emit_code_uint32(char *e, enum rpn_code code, uint32_t v) {
    e[0] = (char)code;
    memcpy(e + RPN_CODE_SIZE, &v, sizeof(uint32_t));
}

/**
 * Translate an ASCII opcode to an RPN_CODE + function pointer.
 */
static enum rpn_error compile_operator(char *e, char c) {
    const size_t op = c - 'A';

    if (!(op < num_elem(funcs))) {
        return RPN_ERR_ILLOPC;
    }

    e[0] = RPN_CODE_CALL;
    memcpy(e + RPN_CODE_SIZE, &funcs[op], sizeof(void *));

    return RPN_ERR_OK;
}

struct rpn_expression *rpn_compile(const char *input) {
    struct rpn_expression *expr; /*!< The final "executable" expression. */
    size_t size = 2 * sizeof(rpn_token);
    int labels[RPN_MAX_LABELS] = { 0 }; /*!< Jump labels. */

    expr = selva_malloc(sizeof(struct rpn_expression));
    memset(expr->literal_reg, 0, sizeof(expr->literal_reg));
    expr->expression = selva_malloc(size);

    if (compile_find_labels(labels, input)) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to parse labels");
        goto fail;
    }

    const char *delim = RPN_DELIM;
    const char *group = RPN_GROUP;
    uint32_t literal_reg_i = 0;
    size_t i = 0;
    size_t tok_len = 0;
    const char *rest = NULL;
    for (const char *tok_str = tokenize(input, delim, group, &rest, &tok_len);
         tok_str != NULL;
         tok_str = tokenize(NULL, delim, group, &rest, &tok_len)) {
        enum rpn_error err;
        char *e = expr->expression[i++];
        rpn_token *new;

        /*
         * Check if there is a label and skip it.
         */
        tok_len = compile_skip_label(&tok_str, tok_len);

        if (tok_len == 0) {
            SELVA_LOG(SELVA_LOGL_ERR, "Token length can't be zero");
            goto fail;
        }

        switch (tok_str[0]) {
        case '#': /* Number literal */
            err = compile_num_literal(expr, literal_reg_i, tok_str + 1, tok_len - 1);
            break;
        case '"': /* String literal */
            err = compile_str_literal(expr, literal_reg_i, tok_str + 1, tok_len - 2);
            break;
        case '{': /* Set literal */
            err = compile_selvaset_literal(expr, literal_reg_i, tok_str + 1, tok_len - 2);
            break;
        case '@':
            compile_emit_code_uint32(e, RPN_CODE_GET_REG_NUMBER, fast_atou(tok_str + 1));
            goto next;
        case '$':
            compile_emit_code_uint32(e, RPN_CODE_GET_REG_STRING, fast_atou(tok_str + 1));
            goto next;
        case '&':
            compile_emit_code_uint32(e, RPN_CODE_GET_REG_SET, fast_atou(tok_str + 1));
            goto next;
        case '>': /* Conditional jump forward */
            if (tok_len < 2 || tok_str[1] == '-') {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid conditional jump");
                goto fail;
            } else {
                unsigned l = fast_atou(tok_str + 1);
                int n;

                if (l >= RPN_MAX_LABELS) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Invalid label");
                    goto fail;
                }

                n = labels[l];
                if (n == 0) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Label not found: %d", n);
                    goto fail;
                }
                if (n <= (int)i - 1) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Can't jump backwards to %d with `>`", n);
                    goto fail;
                }

                compile_emit_code_uint32(e, RPN_CODE_JMP_FWD, n - i);
            }
            goto next;
        default:
            if (tok_len > RPN_MAX_TOKEN_SIZE - 1) {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid token length %llu", (unsigned long long)tok_len);
                goto fail;
            }

            if (compile_operator(e, tok_str[0])) {
                SELVA_LOG(SELVA_LOGL_ERR, "Invalid operator: %c", tok_str[0]);
                goto fail;
            }

            goto next;
        }

        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "RPN compilation error: %s",
                      err >= 0 && err < num_elem(rpn_str_error) ? rpn_str_error[err] : "Unknown");
            goto fail;
        }

        /*
         * Continue literal handling.
         * All literals can be marked with the same prefix from now on as
         * fetching the value will happen with the same function.
         */
        compile_emit_code_uint32(e, RPN_CODE_GET_LIT, literal_reg_i);
        literal_reg_i++;
next:
        new = selva_realloc(expr->expression, size);
        expr->expression = new;

        size += sizeof(rpn_token);
    }

    /* The returned length is only ever 0 if the grouping failed. */
    if (tok_len == 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Tokenization failed");
        rpn_destroy_expression(expr);
        return NULL;
    }

    /* An empty token acts as a terminator for the expression. */
    memset(expr->expression[i], 0, sizeof(rpn_token));

    return expr;
fail:
    rpn_destroy_expression(expr);
    return NULL;
}

void rpn_destroy_expression(struct rpn_expression *expr) {
    if (expr) {
        struct rpn_operand **op = expr->literal_reg;

        selva_free(expr->expression);

        while (*op) {
            (*op)->flags.regist = 0;
            free_rpn_operand(op);
            op++;
        }

        selva_free(expr);
    }
}

void _rpn_auto_free_expression(void *p) {
    struct rpn_expression *expr = *(void **)p;

    rpn_destroy_expression(expr);
}

/**
 * Get a register value and check that it can be used as `type`.
 */
static enum rpn_error rpn_get_reg(struct rpn_ctx *ctx, const char *s, int type) {
    uint32_t i;
    struct rpn_operand *r;

    memcpy(&i, s + RPN_CODE_SIZE, sizeof(uint32_t));
    if (i >= (typeof(i))ctx->nr_reg) {
        SELVA_LOG(SELVA_LOGL_ERR, "Register index out of bounds: %u", (unsigned)i);
        return RPN_ERR_BNDS;
    }

    r = ctx->reg[i];
    if (!r) {
        SELVA_LOG(SELVA_LOGL_ERR, "Register value is a NULL pointer: %u", (unsigned)i);
        return RPN_ERR_NPE;
    }

    if (type == RPN_LVTYPE_NUMBER) {
        if (isnan(r->d)) {
            SELVA_LOG(SELVA_LOGL_ERR, "Register value is not a number: %u", (unsigned)i);
            return RPN_ERR_NAN;
        }

        return push(ctx, r);
    } else if (type == RPN_LVTYPE_STRING) {
        return push(ctx, r);
    } else if (type == RPN_LVTYPE_SET) {
        if (!r->flags.slvset) {
            return RPN_ERR_TYPE;
        }

        return push(ctx, r);
    }

    SELVA_LOG(SELVA_LOGL_ERR, "Unknown type code: %d", type);

    return RPN_ERR_TYPE;
}

/**
 * Get a value from the literal register file.
 * There is no type checking here because we already know that the type is
 * matching the original input.
 */
static enum rpn_error get_literal(struct rpn_ctx *ctx, const struct rpn_expression *expr, const char *s) {
    uint32_t i;

    memcpy(&i, s + RPN_CODE_SIZE, sizeof(uint32_t));

    return push(ctx, expr->literal_reg[i]);
}

/**
 * Conditional jump forward.
 */
static enum rpn_error cond_jump(struct rpn_ctx *ctx, const char *s, const rpn_token * restrict *it) {
    OPERAND(ctx, cond);

    if (to_bool(cond)) {
        uint32_t n;

        memcpy(&n, s + RPN_CODE_SIZE, sizeof(uint32_t));

        for (uint32_t i = 0; i < n; i++) {
            if (***it) {
                (*it)++;
            } else {
                break;
            }
        }
    }

    return RPN_ERR_OK;
}

static enum rpn_error rpn(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr) {
    const rpn_token *it = expr->expression;
    const char *s;

    while (*(s = *it++)) {
#define LBL_OFF(label) (label - &&rpn_code_stop)
        static const ptrdiff_t op_arr[] = {
            [RPN_CODE_STOP] = 0,
            [RPN_CODE_CALL] = LBL_OFF(&&rpn_code_call),
            [RPN_CODE_GET_REG_NUMBER] = LBL_OFF(&&rpn_code_get_reg_number),
            [RPN_CODE_GET_REG_STRING] = LBL_OFF(&&rpn_code_get_reg_string),
            [RPN_CODE_GET_REG_SET] = LBL_OFF(&&rpn_code_get_reg_set),
            [RPN_CODE_GET_LIT] = LBL_OFF(&&rpn_code_get_lit),
            [RPN_CODE_JMP_FWD] = LBL_OFF(&&rpn_code_jmp_fwd),
        };
#undef LBL_OFF
        const long i = s[0];
        rpn_fp fp;
        enum rpn_error err;

        assert(i < (long)num_elem(op_arr));
        goto *(&&rpn_code_stop + op_arr[i]);

rpn_code_stop: /* Must be first. */
rpn_code_call:
        memcpy(&fp, s + RPN_CODE_SIZE, sizeof(void *));
        err = fp(redis_ctx, ctx);
        goto end;
rpn_code_get_reg_number:
        err = rpn_get_reg(ctx, s, RPN_LVTYPE_NUMBER);
        goto end;
rpn_code_get_reg_string:
        err = rpn_get_reg(ctx, s, RPN_LVTYPE_STRING);
        goto end;
rpn_code_get_reg_set:
        err = rpn_get_reg(ctx, s, RPN_LVTYPE_SET);
        goto end;
rpn_code_get_lit:
        err = get_literal(ctx, expr, s);
        goto end;
rpn_code_jmp_fwd:
        err = cond_jump(ctx, s, &it);
end:
        if (err) {
            if (err == RPN_ERR_BREAK) {
                /*
                 * A breaking condition. This is currently reserved for the
                 * modal logic operators. We just return whatever was last
                 * in the stack.
                 */
                OPERAND(ctx, x);

                clear_stack(ctx);
                push(ctx, x);

                break;
            }
            clear_stack(ctx);

            return err;
        }
    }

    if (ctx->depth != 1) {
        clear_stack(ctx);
        return RPN_ERR_BADSTK;
    }

    return RPN_ERR_OK;
}

enum rpn_error rpn_bool(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, int *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = to_bool(res);
    free_rpn_operand(&res);

    return RPN_ERR_OK;
}

enum rpn_error rpn_double(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, double *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = res->d;
    free_rpn_operand(&res);

    return RPN_ERR_OK;
}

enum rpn_error rpn_integer(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, long long *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = (long long)round(res->d);
    free_rpn_operand(&res);

    return RPN_ERR_OK;
}

enum rpn_error rpn_rms(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, RedisModuleString **out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    *out = RedisModule_CreateString(redis_ctx, OPERAND_GET_S(res), OPERAND_GET_S_LEN(res));
    free_rpn_operand(&res);

    return *out ? RPN_ERR_OK : RPN_ERR_ENOMEM;
}

enum rpn_error rpn_selvaset(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_expression *expr, struct SelvaSet *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    if (err) {
        return err;
    }

    res = pop(ctx);
    if (!res) {
        return RPN_ERR_BADSTK;
    }

    if (res->flags.slvset && res->set) {
        if (res->flags.regist) {
            err = SelvaSet_Union_err2rpn_error(SelvaSet_Union(out, res->set, NULL));
        } else {
            /* Safe to move the strings. */
            err = SelvaSet_Union_err2rpn_error(SelvaSet_Merge(out, res->set));
        }
    } else if (to_bool(res)) {
        /* However, if res is falsy we interpret it as meaning an empty set. */
        err = RPN_ERR_TYPE;
    }
    free_rpn_operand(&res);

    return err;
}
