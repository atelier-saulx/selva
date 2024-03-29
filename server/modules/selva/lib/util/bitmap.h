/*
 * Copyright (c) 2021 SAULX
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#ifndef BITMAP_H
#define BITMAP_H

#include <stddef.h>
#include <stdint.h>
#include "cdefs.h"

#if __SIZEOF_INT128__ != 16
#error The compiler and architecture must have Tetra-Integer support
#endif

typedef unsigned __int128 bitmap_t;

/**
 * A variable size bitmap.
 */
struct bitmap {
    size_t nbits;
    bitmap_t d[0];
};

#define BITMAP_CEILING(x, y) \
    (((x) + (y) - (size_t)1) / (y))

/**
 * Byte size of a bitmap struct passable to a malloc()-like function.
 */
#define BITMAP_ALLOC_SIZE(nbits) \
    (sizeof(struct bitmap) + BITMAP_CEILING(max((size_t)(nbits), (size_t)8) / (size_t)8, sizeof(bitmap_t)) * sizeof(bitmap_t))

/**
 * Get the status of a bit in a bitmap pointed by bitmap.
 * @param bitmap            is a pointer to a bitmap.
 * @param pos               is the bit position to be checked.
 * @return  Boolean value or -1.
 */
int bitmap_get(const struct bitmap *bitmap, size_t pos);

/**
 * Set a bit in a bitmap pointed by bitmap.
 * @param bitmap            is a pointer to a bitmap.
 * @param pos               is the bit position to be set.
 * @return  0 or -1.
 */
int bitmap_set(struct bitmap *bitmap, size_t pos);

/**
 * Clear a bit in a bitmap pointed by bitmap.
 * @param bitmap            is a pointer to a bitmap.
 * @param pos               is the bit position to be cleared.
 * @return  0 or -1.
 */
int bitmap_clear(struct bitmap *bitmap, size_t pos);

/**
 * Erase the whole bitmap.
 * @param bitmap            is a pointer to a bitmap.
 */
void bitmap_erase(struct bitmap *bitmap);

long long bitmap_popcount(const struct bitmap *bitmap);

/**
 * Find first set.
 */
int bitmap_ffs(const struct bitmap *bitmap);

#endif /* BITMAP_H */
