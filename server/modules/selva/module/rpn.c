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
#include "selva_node.h"
#include "selva_object.h"
#include "selva_set.h"
#include "rpn.h"

#define RPN_ASSERTS         0

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
#define RMSTRING_TEMPLATE "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

#define OPERAND(ctx, x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = pop(ctx); \
    if (!x) return RPN_ERR_BADSTK

#define OPERAND_GET_S(x) \
     ((const char *)((x)->flags.spused && ((x)->sp) ? (x)->sp : (x)->s))

#define OPERAND_GET_S_LEN(x) \
    ((x)->s_size > 0 ? (x)->s_size - 1 : 0)

/**
 * Use to create an empty result operand pointer.
 */
#define RESULT_OPERAND(x) \
    struct rpn_operand * x __attribute__((cleanup(free_rpn_operand))) = NULL

struct rpn_operand {
    struct {
        unsigned in_use : 1; /*!< In use/in stack, do not free. */
        unsigned pooled : 1; /*!< Pooled operand, do not free. */
        unsigned regist : 1; /*!< Register value, do not free. */
        unsigned spused : 1; /*!< The value is a string pointed by sp. */
        unsigned spfree : 1; /*!< Free sp if set when freeing the operand. */
        unsigned slvset : 1; /*!< Set pointer is pointing to a SelvaSet. */
    } flags;
    struct rpn_operand *next_free; /* Next free in pool. */
    size_t s_size;
    double d;
    union {
        const char *sp; /*!< A pointer to a user provided string. */
        struct SelvaSet *set; /*! A pointer to a SelvaSet. */
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

    ctx = RedisModule_Alloc(sizeof(struct rpn_ctx));
    if (unlikely(!ctx)) {
        return NULL;
    }

    ctx->depth = 0;
    ctx->redis_hkey = NULL;
    ctx->obj = NULL;
    ctx->rms_id = NULL;
    ctx->rms_field = NULL;
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
        clear_stack(ctx);
        for (int i = 0; i < ctx->nr_reg; i++) {
            struct rpn_operand *v = ctx->reg[i];

            if (!v) {
                continue;
            }

            v->flags.in_use = 0;
            v->flags.regist = 0;

            free_rpn_operand(&v);
        }

        RedisModule_Free(ctx->rms_id);
        RedisModule_Free(ctx->rms_field);
        RedisModule_Free(ctx->reg);
        RedisModule_Free(ctx);
    }
}

/**
 * Copy C string to RedisModuleString.
 * Be careful to not change something that is already referenced somewhere.
 */
static int cpy2rm_str(RedisModuleString **rms_p, const char *str, size_t len) {
    RedisModuleString *rms;

    if (!*rms_p) {
        *rms_p = RedisModule_CreateString(NULL, RMSTRING_TEMPLATE, sizeof(RMSTRING_TEMPLATE));
        if (unlikely(!*rms_p)) {
            return RPN_ERR_ENOMEM;
        }
    }

    rms = *rms_p;
    if (((struct redisObjectAccessor *)rms)->refcount > 1) {
        fprintf(stderr, "%s:%d:, The given RMS (%p) is already in use and cannot be modified\n",
                __FILE__, __LINE__, rms);
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

static int rpn_operand2rms(RedisModuleString **rms, struct rpn_operand *o) {
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

    if (v->flags.spused && v->flags.spfree && v->sp) {
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

__used static int isnan_undefined(double x) {
    long long i;

    if (!isnan(x)) {
      return 0;
    }

    memcpy(&i, &x, sizeof(i));

    return i & 1;
}

static enum rpn_error push(struct rpn_ctx *ctx, struct rpn_operand *v) {
	if (unlikely(ctx->depth >= RPN_MAX_D)) {
        fprintf(stderr, "%s:%d: Stack overflow\n", __FILE__, __LINE__);
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

    return push(ctx, v);
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
        free_rpn_operand(&v);
    }
}

static int to_bool(struct rpn_operand *v) {
    const double d = v->d;

    if (isnan(d)) {
        if (v->flags.slvset) {
            return v->set && v->set->size > 0;
        }
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

        r = alloc_rpn_operand(sizeof(char *));
        if (!r) {
            return RPN_ERR_ENOMEM;
        }

        /*
         * Set the string value.
         */
        r->flags.regist = 1; /* Can't be freed when this flag is set. */
        r->flags.spused = 1;
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

enum rpn_error rpn_set_reg_rm(struct rpn_ctx *ctx, size_t i, RedisModuleString *rms) {
    TO_STR(rms);
    const size_t size = rms_len + 1;
    char *arg;

    arg = RedisModule_Alloc(size);
    if (!arg) {
        return RPN_ERR_ENOMEM;
    }

    memcpy(arg, rms_str, size);
    return rpn_set_reg(ctx, i, arg, size, RPN_SET_REG_FLAG_RMFREE);
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

static enum rpn_error rpn_get_reg(struct rpn_ctx *ctx, const char *str_index, int type) {
    const size_t i = fast_atou(str_index);

    if (i >= (size_t)ctx->nr_reg) {
        fprintf(stderr, "%s:%d: Register index out of bounds: %zu\n",
                __FILE__, __LINE__, i);
        return RPN_ERR_BNDS;
    }

    struct rpn_operand *r = ctx->reg[i];

    if (!r) {
        fprintf(stderr, "%s:%d: Register value is a NULL pointer: %zu\n",
                __FILE__, __LINE__, i);
        return RPN_ERR_NPE;
    }

    if (type == RPN_LVTYPE_NUMBER) {
        if (isnan(r->d)) {
            fprintf(stderr, "%s:%d: Register value is not a number: %zu\n",
                    __FILE__, __LINE__, i);
            return RPN_ERR_NAN;
        }

        return push(ctx, r);
    } else if (type == RPN_LVTYPE_STRING) {
        return push(ctx, r);
    }

    fprintf(stderr, "%s:%d: Unknown type code: %d\n",
            __FILE__, __LINE__, type);

    return RPN_ERR_TYPE;
}

static struct SelvaObject *open_node_object(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    struct SelvaObject *obj;

    if (ctx->obj) {
        obj = ctx->obj;
    } else {
        RedisModuleKey *key;

#if RPN_ASSERTS
        assert(redis_ctx);
#endif
        if (!ctx->reg[0]) {
            return NULL;
        }

        /* Current node_id is always stored in reg[0] */
        const char *id_str = OPERAND_GET_S(ctx->reg[0]);
        int err = cpy2rm_str(&ctx->rms_id, id_str, strnlen(id_str, SELVA_NODE_ID_SIZE));
        if (err) {
            return NULL;
        }

        key = RedisModule_OpenKey(redis_ctx, ctx->rms_id, REDISMODULE_READ | REDISMODULE_OPEN_KEY_NOTOUCH);
        if (!key) {
            return NULL;
        }

        ctx->redis_hkey = key;
        err = SelvaObject_Key2Obj(key, &obj);
        if (err) {
            return NULL;
        }
        ctx->obj = obj;
    }

    return obj;
}

static enum rpn_error rpn_getfld(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const struct rpn_operand *field, int type) {
    struct SelvaObject *obj;
    const char *field_str = OPERAND_GET_S(field);
    const size_t field_len = OPERAND_GET_S_LEN(field);
    RedisModuleString *value = NULL;
    int err;

    obj = open_node_object(redis_ctx, ctx);
    if (!obj) {
        fprintf(stderr, "%s:%d: Node object not found for: \"%.*s\"\n",
                __FILE__, __LINE__,
                (int)SELVA_NODE_ID_SIZE, RedisModule_StringPtrLen(ctx->rms_id, NULL));
        return RPN_ERR_NPE;
    }

    const enum SelvaObjectType field_type = SelvaObject_GetTypeStr(obj, field_str, field_len);
    if (field_type == SELVA_OBJECT_NULL) {
        return (type == RPN_LVTYPE_NUMBER) ? push_double_result(ctx, nan_undefined()) : push_empty_value(ctx);
    } else if (field_type == SELVA_OBJECT_SET) {
        struct SelvaSet *set;

        set = SelvaObject_GetSetStr(obj, field_str, field_len);
        if (!set) {
            return push_empty_value(ctx);
        }

        /* TODO We should validate the subtype */

        return push_selva_set_result(ctx, set);
    } else { /* Primitive type */
        if (type == RPN_LVTYPE_NUMBER) {
            double dvalue;

            switch (field_type) {
            case SELVA_OBJECT_DOUBLE:
                err = SelvaObject_GetDoubleStr(obj, field_str, field_len, &dvalue);
                break;
            case SELVA_OBJECT_LONGLONG:
                {
                    long long v;

                    err = SelvaObject_GetLongLongStr(obj, field_str, field_len, &v);
                    dvalue = (double)v;
                }
                break;
            default:
                err = RPN_ERR_NAN;
            }

            if (err) {
                const char *type_str = SelvaObject_Type2String(field_type, NULL);

                fprintf(stderr, "%s:%d: Field value [%.*s].%.*s is not a number, actual type: \"%s\"\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, OPERAND_GET_S(ctx->reg[0]),
                        (int)field->s_size, OPERAND_GET_S(field),
                        type_str ? type_str : "INVALID");

                return RPN_ERR_NAN;
            }

            return push_double_result(ctx, dvalue);
        } else { /* Assume SELVA_OBJECT_STRING && RPN_LVTYPE_STRING */
            /* This will fail if the field type is not a string. */
            /* TODO: wrap this in a new function so it takes lang and gets from text if it's a text type object instead of string inn rms_field */
            err = SelvaObject_GetStringStr(obj, field_str, field_len, &value);
            if (err || !value) {
#if 0
                fprintf(stderr, "%s:%d: Field \"%s\" not found for node: \"%.*s\"\n",
                        __FILE__, __LINE__,
                        OPERAND_GET_S(field),
                        (int)SELVA_NODE_ID_SIZE, (const void *)OPERAND_GET_S(ctx->reg[0]));
#endif
                return push_empty_value(ctx);
            }

            /*
             * Supposedly there is no need to free `value`
             * because we are using automatic memory management.
             */
            return push_rm_string_result(ctx, value);
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
    double d = b->d;

    return push_double_result(ctx, a->d / d);
}

static enum rpn_error rpn_op_mul(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);

    return push_double_result(ctx, a->d * b->d);
}

static enum rpn_error rpn_op_rem(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    long long d = (long long)b->d;

    if (d == 0) {
        return RPN_ERR_DIV;
    }

    return push_int_result(ctx, (long long)a->d % d);
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

    if (!to_bool(a)) {
        return RPN_ERR_NECESS;
    }

    return push_double_result(ctx, 1.0);
}

static enum rpn_error rpn_op_exists(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    int exists;
    struct SelvaObject *obj;
    OPERAND(ctx, field);
    const char *field_str = OPERAND_GET_S(field);
    const size_t field_len = OPERAND_GET_S_LEN(field);

    obj = open_node_object(redis_ctx, ctx);
    if (!obj) {
        return push_double_result(ctx, 0.0);
    }

    exists = !SelvaObject_ExistsStr(obj, field_str, field_len);

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

    if (s->flags.slvset) {
        /* The operand `s` is a set. */
        set = s->set;
    } else {
        /* The operand `s` is a field_name string. */
        struct SelvaObject *obj;

        obj = open_node_object(redis_ctx, ctx);
        if (!obj) {
            fprintf(stderr, "%s:%d: Node object not found for: \"%.*s\"\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, RedisModule_StringPtrLen(ctx->rms_id, NULL));
            return push_int_result(ctx, 0);
        }

        const char *field_name_str = OPERAND_GET_S(s);
        const size_t field_name_len = OPERAND_GET_S_LEN(s);
        set = SelvaObject_GetSetStr(obj, field_name_str, field_name_len);
        if (!set) {
            return push_int_result(ctx, 0);
        }
    }

    if (set->type == SELVA_SET_TYPE_RMSTRING) {
        /*
         * We use rms_field here because we don't need it for the field_name in this
         * function.
         */
        if(rpn_operand2rms(&ctx->rms_field, v)) {
            return RPN_ERR_ENOMEM;
        }

        return push_int_result(ctx, SelvaSet_Has(set, ctx->rms_field));
    } else if (set->type == SELVA_SET_TYPE_DOUBLE) {
        return push_int_result(ctx, SelvaSet_Has(set, v->d));
    } else if (set->type == SELVA_SET_TYPE_LONGLONG) {
        return push_int_result(ctx, SelvaSet_Has(set, (long long)v->d));
    } else {
        return push_int_result(ctx, 0);
    }
}

static enum rpn_error rpn_op_typeof(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
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

static enum rpn_error rpn_op_strcmp(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx) {
    OPERAND(ctx, a);
    OPERAND(ctx, b);
    const size_t a_size = a->s_size;
    const size_t b_size = b->s_size;
    const ssize_t sizeDiff = b_size - a_size;

    return push_int_result(ctx, !sizeDiff &&
                           !strncmp(OPERAND_GET_S(a),
                                    OPERAND_GET_S(b),
                                    size_min(a_size, b_size)));
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
     * Note the the allocated string is always large so that the comparison is safe.
     */

    return push_int_result(ctx, !memcmp(OPERAND_GET_S(a), OPERAND_GET_S(ctx->reg[0]), SELVA_NODE_TYPE_SIZE));
}

static enum rpn_error rpn_op_getsfld(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(redis_ctx, ctx, field, RPN_LVTYPE_STRING);
}

static enum rpn_error rpn_op_getdfld(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx) {
    OPERAND(ctx, field);

    return rpn_getfld(redis_ctx, ctx, field, RPN_LVTYPE_NUMBER);
}

static enum rpn_error rpn_op_abo(struct RedisModuleCtx *redis_ctx __unused, struct rpn_ctx *ctx __unused) {
    return RPN_ERR_ILLOPC;
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
    rpn_op_has,     /* a */
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

    char *rest;
    for (const char *s = strtok_r(sa, w, &rest);
         s != NULL;
         s = strtok_r(NULL, w, &rest)) {
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

static enum rpn_error read_num_literal(struct rpn_ctx *ctx, const char *str) {
    char *e;
    const double d = strtod(str, &e);
    RESULT_OPERAND(v);

    if (unlikely(e == str)) {
        fprintf(stderr, "%s:%d: Operand is not a number: #%s\n",
                __FILE__, __LINE__, str);
        return RPN_ERR_NAN;
    }

    v = alloc_rpn_operand(0);
    v->d = d;
    v->s_size = 0;
    v->s[0] = '\0';

    return push(ctx, v);
}

static enum rpn_error read_str_literal(struct rpn_ctx *ctx, const char *str) {
    size_t size = strlen(str) + 1;
    RESULT_OPERAND(v);

#ifdef RPN_ASSERTS
    /* We don't expect to see extra long strings here. */
    assert(size <= 120);
#endif

    v = alloc_rpn_operand(size);
    v->s_size = size;
    strncpy(v->s, str, size);
    v->s[size - 1] = '\0';
    v->d = nan("");

    return push(ctx, v);
}

static enum rpn_error rpn(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const rpn_token *expr) {
    const rpn_token *it = expr;
    const char *s;

    while (*(s = *it++)) {
        size_t op = *s - 'A';

        if (op < sizeof(funcs) / sizeof(void *)) { /* Operator */
            enum rpn_error err;
            err = funcs[op](redis_ctx, ctx);
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
            enum rpn_error err;

            switch (s[0]) {
            case '@':
                err = rpn_get_reg(ctx, s + 1, RPN_LVTYPE_NUMBER);
                break;
            case '$':
                err = rpn_get_reg(ctx, s + 1, RPN_LVTYPE_STRING);
                break;
            case '#':
                err = read_num_literal(ctx, s + 1);
                break;
            case '"':
                err = read_str_literal(ctx, s + 1);
                break;
            default:
                fprintf(stderr, "%s:%d: Illegal operand: \"%s\"\n",
                        __FILE__, __LINE__, s);
                err = RPN_ERR_ILLOPN;
            }

            if (err) {
                clear_stack(ctx);
                return err;
            }
        }
	}

	if (ctx->depth != 1) {
        clear_stack(ctx);
        return RPN_ERR_BADSTK;
    }

    return RPN_ERR_OK;
}

static void close_node_key(struct rpn_ctx *ctx) {
    if (ctx->redis_hkey) {
        RedisModule_CloseKey(ctx->redis_hkey);
    }
    ctx->redis_hkey = NULL;
    ctx->obj = NULL;
}

enum rpn_error rpn_bool(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const rpn_token *expr, int *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    close_node_key(ctx);
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

enum rpn_error rpn_double(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const rpn_token *expr, double *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    close_node_key(ctx);
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

enum rpn_error rpn_integer(struct RedisModuleCtx *redis_ctx, struct rpn_ctx *ctx, const rpn_token *expr, long long *out) {
    struct rpn_operand *res;
    enum rpn_error err;

    err = rpn(redis_ctx, ctx, expr);
    close_node_key(ctx);
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
