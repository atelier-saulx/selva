/*
 * Copyright (c) 2021-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef AUTO_FREE_H
#define AUTO_FREE_H

/**
 * Wrap selva_free().
 */
void _wrap_selva_free(void *p);

/**
 * Pointer variable attribute to free the object pointed by the pointer.
 * The value must have been allocated with one of the selva_ allocation
 * functions.
 */
#define __selva_autofree __attribute__((cleanup(_wrap_selva_free)))

#endif /* AUTO_FREE_H */
