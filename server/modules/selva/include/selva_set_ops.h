#pragma once
#ifndef _SELVA_SET_OPS_H_
#define _SELVA_SET_OPS_H_

/*
 * Set operations and cross compatibility between SelvaSet and other set-like
 * data structures and Selva node fields.
 */

struct RedisModuleCtx;
struct SelvaHierarchy;
struct SelvaHierarchyNode;
struct SelvaSet;

/*
 * @addtogroup selva_set_has
 * Test if a set-like field has an element.
 * @{
 */

/**
 * Test if a set-like field has a string value.
 * @returns Boolean.
 */
int SelvaSet_field_has_string(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        const char *value_str,
        size_t value_len);

/**
 * Test if a set-like field has a double value.
 * @returns Boolean.
 */
int SelvaSet_field_has_double(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        double value);

/**
 * Test if a set-like field has a long long value.
 * @returns Boolean.
 */
int SelvaSet_field_has_longlong(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_str,
        size_t field_len,
        long long value);

/**
 * @}
 */

/*
 * @addtogroup selva_set_subset
 * Test if A is a subset of B.
 * @{
 */

/**
 * Test if the set a is a subset of the set b.
 */
int SelvaSet_seta_in_setb(struct SelvaSet *a, struct SelvaSet *b);

/**
 * Test if the set a is a subset of the set-like field b.
 */
int SelvaSet_seta_in_fieldb(
        struct RedisModuleCtx *ctx,
        struct SelvaSet *a,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_b_str,
        size_t field_b_len);

/**
 * Test if the set-like field a is a subset of set b.
 */
int SelvaSet_fielda_in_setb(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        struct SelvaSet *b);

/**
 * Test if the set-like field a is a subset of the set-like field b.
 */
int SelvaSet_fielda_in_fieldb(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaHierarchyNode *node,
        const char *field_a_str,
        size_t field_a_len,
        const char *field_b_str,
        size_t field_b_len);

/**
 * @}
 */

#endif /* _SELVA_SET_OPS_H_ */
