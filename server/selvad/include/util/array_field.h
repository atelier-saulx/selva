/*
 * Copyright (c) 2020-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_ARRAY_FIELD_H_
#define _UTIL_ARRAY_FIELD_H_

struct SVector;

ssize_t get_array_field_index(const char *field_str, size_t field_len, ssize_t *res) __attribute__((access(read_only, 1, 2), access(write_only, 3)));

/**
 * Convert integer index to unsigned absolute index.
 * Negative index starts counting from the end of the array.
 */
size_t ary_idx_to_abs(ssize_t len, ssize_t ary_idx);

/**
 * Convert integer index to unsigned absolute index.
 * Negative index starts counting from the end of the vector.
 */
size_t vec_idx_to_abs(struct SVector *vec, ssize_t ary_idx);

#endif /* _UTIL_ARRAY_FIELD_H_ */
