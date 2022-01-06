/*
 * Copyright (c) 2021 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _SELVA_PTAG_H_
#define _SELVA_PTAG_H_

/*
 * Tagged pointers.
 */

#define PTAG_MASK ~0x03ull

/**
 * Create a tagged pointer.
 */
#define PTAG(value, tag) \
    ((typeof (value))(((uintptr_t)(value) & PTAG_MASK) | (tag)))

/**
 * Get the tag value from a tagged pointer.
 */
#define PTAG_GETTAG(ptag) \
    ((uintptr_t)(ptag) & 0x03ull)

/**
 * Get the pointer value from a tagged pointer.
 */
#define PTAG_GETP(ptag) \
    (void *)((uintptr_t)ptag & PTAG_MASK)

#endif /* _SELVA_PTAG_H_ */
