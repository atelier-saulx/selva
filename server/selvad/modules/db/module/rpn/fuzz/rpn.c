// Copyright (c) 2022 SAULX
//
// SPDX-License-Identifier: MIT

#include <alloca.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "util/cstrings.h"
#include "selva_log.h"
#include "rpn.h"

void *selva_malloc(size_t n)
{
    return malloc(n);
}

void *selva_calloc(size_t n, size_t s)
{
    return calloc(n, s);
}

void *selva_realloc(void *ptr, size_t new_size)
{
    return realloc(ptr, new_size);
}

void selva_free(void *p)
{
    free(p);
}

const char *selva_strerror(int err)
{
    return "";
}

int SelvaSet_field_has_string(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        const char *value_str,
        size_t value_len)
{
    return 0;
}

/**
 * Test if a set-like field has a double value.
 * @returns Boolean.
 */
int SelvaSet_field_has_double(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        double value)
{
    return 0;
}

/**
 * Test if a set-like field has a long long value.
 * @returns Boolean.
 */
int SelvaSet_field_has_longlong(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        long long value)
{
    return 0;
}

/**
 * Test if the set a is a subset of the set-like field b.
 * @returns Boolean.
 */
int SelvaSet_seta_in_fieldb(
        struct SelvaSet *a,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_b_str,
        size_t field_b_len)
{
    return 0;
}

/**
 * Test if the set-like field a is a subset of set b.
 * @returns Boolean.
 */
int SelvaSet_fielda_in_setb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        struct SelvaSet *b)
{
    return 0;
}

/**
 * Test if the set-like field a is a subset of the set-like field b.
 * @returns Boolean.
 */
int SelvaSet_fielda_in_fieldb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        const char *field_b_str,
        size_t field_b_len)
{
    return 0;
}

int SelvaHierarchy_IsNonEmptyField(const struct SelvaHierarchyNode *node, const char *field_str, size_t field_len)
{
    return 0;
}

struct SelvaHierarchyMetadata *_SelvaHierarchy_GetNodeMetadataByPtr(struct SelvaHierarchyNode *node)
{
    return NULL;
}

uint32_t crc32c(uint32_t crc, void const *buf, size_t len)
{
    return 0;
}

long long ts_now(void)
{
    return 0;
}

void _selva_log(enum selva_log_level level, const char * restrict where, const char * restrict func, const char * restrict fmt, ...)
{
}

static struct rpn_expression *compile(const char *input, size_t len)
{
    char *str = alloca(len + 1);

    memcpy(str, input, len);
    str[len] = '\0';
    return rpn_compile(str);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
#define REGS 5
    struct rpn_ctx *ctx;
    struct rpn_expression *expr;
    int out;

    if (Size < REGS) {
        return 0;
    }

    ctx = rpn_init(REGS);

    for (int i = 0; i < REGS; i++) {
        const char *v = (const char *)Data + i;

        rpn_set_reg(ctx, i, v, 1, i == 0 || v[0] % i ? 0 : RPN_SET_REG_FLAG_IS_NAN);
    }

    expr = compile((const char *)Data + REGS, Size - REGS);
    if (expr) {
        (void)rpn_bool(ctx, expr, &out);
    }
    rpn_destroy_expression(expr);
    rpn_destroy(ctx);

    return 0;
}

__constructor static void init(void)
{
    selva_log = _selva_log;
}
