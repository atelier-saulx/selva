/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "queue.h"

/**
 * Item in the finalizer list.
 * The dispose function will be called for the pointer p when finalizer_run()
 * is called.
 */
struct finalizer_item {
    void *p;
    void (*dispose)(void *p);
    SLIST_ENTRY(finalizer_item) entries;
};

SLIST_HEAD(finalizer_stack, finalizer_item);

/**
 * A LIFO finalizer structure.
 */
struct finalizer {
    struct finalizer_stack head;
};

/**
 * Initializer a finalizer.
 */
void finalizer_init(struct finalizer *fin);

/**
 * Add a new pointer to be finalizer with a dispose function.
 * LIFO.
 */
void finalizer_add(struct finalizer *fin, void *p, void (*dispose)(void *p));

/**
 * Delete a pointer from the finalizer.
 * Cancels the finalization.
 */
void finalizer_del(struct finalizer *fin, void *p);

/**
 * Runt he finalizer for the items currently in the finalization list.
 * Runs LIFO.
 */
void finalizer_run(struct finalizer *fin);

void _wrap_finalizer_run(void *p);

#define __auto_finalizer __attribute__((cleanup(_wrap_finalizer_run)))
