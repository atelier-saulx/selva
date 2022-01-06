/*
 * Copyright (c) 2021 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _FUNMAP_H_
#define _FUNMAP_H_

/**
 * Generate an extern prototype for function mapping a selector to a function pointer.
 */
#define GENERATE_FUNMAP_PROTO(name, funcs, selector_type) \
    typeof(*(funcs)) name(selector_type v);

/**
 * Generate a static prototype for function mapping a selector to a function pointer.
 */
#define GENERATE_STATIC_FUNMAP_PROTO(name, funcs, selector_type) \
    static GENERATE_FUNMAP_PROTO(name, funcs, selector_type);

/**
 * Generate an extern function mapping selector to a function pointer definition.
 */
#define GENERATE_FUNMAP(name, funcs, selector_type, default_index) \
    typeof(*(funcs)) name(selector_type v) { \
        return funcs[((long unsigned int)v >= (sizeof(funcs) / sizeof(*(funcs)))) \
            ? (long unsigned int)(default_index) : (long unsigned int)v]; \
    }

/**
 * Generate a static function mapping selector to a function pointer definition.
 */
#define GENERATE_STATIC_FUNMAP(name, funcs, selector_type, default_index) \
    static GENERATE_FUNMAP(name, funcs, selector_type, default_index)

#endif /* _FUNMAP_H_ */
