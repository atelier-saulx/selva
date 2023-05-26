/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

typedef void hll_t;

/**
 * Create a new HyperLogLog data structure.
 */
hll_t *hll_create(void);

/**
 * Stringify a HyperLogLog.
 * The return value must not be freed as it's using the same storage as `ptr`.
 */
const char *hll_getstr(hll_t *ptr, size_t *size);

/**
 * Restore a previously stringified HyperLogLog.
 * `data` can be freed after this call.
 */
hll_t *hll_restore(const char *data, size_t size);

/**
 * Destroy a HyperLogLog data structure.
 */
void hll_destroy(hll_t *ptr);

/**
 * Size of a HyperLogLog data structure in bytes.
 */
size_t hll_bsize(hll_t *ptr);

/**
 * Add an element to the HyperLogLog data structure.
 */
int hll_add(hll_t **ptr, const unsigned char *ele, size_t elesize);

/**
 * Galculate the Hyperloglog cardinality.
 */
long long hll_count(hll_t *ptr);

/**
 * Merge the Hyperloglog structure src into dst.
 */
int hll_merge(hll_t **dest, hll_t *src);

/**
 * Convert a Hyperloglog data struct stored in a sparse representation into a dense representation.
 */
hll_t *hll_sparse_to_dense(hll_t *ptr);
