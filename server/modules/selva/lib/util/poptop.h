/*
 * Copyright (c) 2021 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _UTIL_POPTOP_H_
#define _UTIL_POPTOP_H_

struct poptop_list_el {
    float score;
    void *p;
};

struct poptop {
    /**
     * Maximum size of the top list.
     */
    size_t max_size;
    /**
     * Minimum score.
     * A long term minimum score for an element to get into and remain in the
     * data structure. This is updated when poptop_maintenance() is called.
     */
    float cut_limit;
    struct poptop_list_el *list;
};

#define POPTOP_FOREACH(_el_, _l_) \
    _el_ = (_l_)->list - 1; \
    while (++_el_ != ((_l_)->list + (_l_)->max_size))

/**
 * Initialize a poptop structure.
 * @param l is a pointer to an uninitalized poptop structure.
 * @param max_size is the maximum number of elements allowed in the data
 * structure. Poptop will attempt to maintain a list of about half of the
 * maximum size.
 * @param initial_cut is an initial value for the cut limit.
 * @returns 0 if succeed; Otherwise a non-zero value is returned.
 */
int poptop_init(struct poptop *l, size_t max_size, float initial_cut);

/**
 * Deinit a poptop structure.
 * @param l is a pointer to an initialized poptop structure.
 */
void poptop_deinit(struct poptop *l);

/**
 * Add an element to the top list if it's above the self-determined score limit.
 * @param l is a pointer to the poptop structure.
 */
void poptop_maybe_add(struct poptop * restrict l, float score, void * restrict p);

/**
 * Remove an element from the top list.
 * @param l is a pointer to the poptop structure.
 */
void poptop_remove(struct poptop * restrict l, const void * restrict p);

/**
 * Periodic maintenance.
 * Find the median score and establish a new cut limit.
 * @param l is a pointer to the poptop structure.
 */
int poptop_maintenance(struct poptop *l);

/**
 * Drop an element that has a score below the cut limit.
 * This function should be called repeatedly until no entry is returned.
 * @param l is a pointer to the poptop structure.
 * @returns a pointer to the element that was removed from the data structure l.
 */
void *poptop_maintenance_drop(struct poptop *l);

#endif /* _UTIL_POPTOP_H_ */
