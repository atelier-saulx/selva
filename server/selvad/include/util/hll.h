/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

typedef void hll_t;

hll_t *hll_create(void);
void hll_destroy(hll_t *ptr);
int hll_add(hll_t **ptr, const unsigned char *ele, size_t elesize);
long long hll_count(hll_t *ptr);
int hll_merge(hll_t **dest, hll_t *src);
hll_t *hll_sparse_to_dense(hll_t *ptr);
