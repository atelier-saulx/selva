/**
 *******************************************************************************
 * @file    bitmap.c
 * @author  Olli Vanhoja
 * @section LICENSE
 * Copyright (c) 2019 Olli Vanhoja <olli.vanhoja@alumni.helsinki.fi>
 * Copyright (c) 2013 - 2016 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

#include <nmmintrin.h>
#include <stddef.h>
#include <string.h>
#include "bitmap.h"

#define SIZEOF_BITMAP_T (8 * sizeof(bitmap_t))

#define BIT2WORDI(i)    ((i - (i & (SIZEOF_BITMAP_T - (size_t)1))) / SIZEOF_BITMAP_T)
#define BIT2WBITOFF(i)  (i & (SIZEOF_BITMAP_T - (size_t)1))
#define BITMAP_SIZE(n) (BITMAP_ALLOC_SIZE(n) - sizeof(struct bitmap))

int bitmap_get(const struct bitmap *bitmap, size_t pos) {
    const size_t k = BIT2WORDI(pos);
    const size_t n = BIT2WBITOFF(pos);

    if (pos >= bitmap->nbits) {
        return -1;
    }

    return (bitmap->d[k] & ((bitmap_t)1 << n)) != (bitmap_t)0;
}

int bitmap_set(struct bitmap *bitmap, size_t pos) {
    const size_t k = BIT2WORDI(pos);
    const size_t n = BIT2WBITOFF(pos);

    if (pos >= bitmap->nbits) {
        return -1;
    }

    bitmap->d[k] |= (bitmap_t)1 << n;

    return 0;
}

int bitmap_clear(struct bitmap *bitmap, size_t pos) {
    const size_t k = BIT2WORDI(pos);
    const size_t n = BIT2WBITOFF(pos);

    if (pos >= bitmap->nbits) {
        return -1;
    }

    bitmap->d[k] &= ~((bitmap_t)1 << n);

    return 0;
}

void bitmap_erase(struct bitmap *bitmap) {
    memset(bitmap->d, 0, BITMAP_SIZE(bitmap->nbits));
}

static inline unsigned int popcnt_u128(__uint128_t n)
{
    const uint64_t n_hi = n >> 64;
    const uint64_t n_lo = n;

    return _mm_popcnt_u64(n_hi) + _mm_popcnt_u64(n_lo);
}

unsigned int bitmap_popcount(const struct bitmap *bitmap) {
    const size_t nbits = bitmap->nbits;
    const bitmap_t *d = bitmap->d;
    unsigned int cnt = 0;

    for (size_t i = 0; i < BITMAP_SIZE(nbits) / sizeof(bitmap_t); i++) {
        cnt += popcnt_u128(*d++);
    }

    return cnt;
}
