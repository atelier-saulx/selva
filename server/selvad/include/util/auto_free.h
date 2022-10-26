/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef AUTO_FREE_H
#define AUTO_FREE_H

/**
 * Wrap RedisModule_Free().
 */
void _wrap_RM_Free(void *p);
void _wrap_selva_free(void *p);

/**
 * Pointer variable attribute to free the object pointed by the pointer.
 * The value must have been allocated with one of the RedisModule allocation
 * functions.
 */
#define __rm_autofree __attribute__((cleanup(_wrap_RM_Free)))

/**
 * Pointer variable attribute to free the object pointed by the pointer.
 * The value must have been allocated with one of the selva_ allocation
 * functions.
 */
#define __selva_autofree __attribute__((cleanup(_wrap_selva_free)))

#endif /* AUTO_FREE_H */
