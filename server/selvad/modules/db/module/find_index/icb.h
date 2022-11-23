/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef _FIND_INDEX_ICB_
#define _FIND_INDEX_ICB_

/*
 * Manage ICB map: name -> ICB.
 */

struct SelvaHierarchy;
struct selva_string;

/**
 * Sorting descriptor for ordered index.
 */
struct index_order {
    enum SelvaResultOrder order; /*!< Index order if applicable. */
    struct selva_string *order_field; /*!< Index order by field. */
};

/**
 * Index descriptor struct for upsert_icb().
 */
struct icb_descriptor {
    enum SelvaTraversal dir; /*!< Indexing traversal direction. */
    struct selva_string *dir_expression; /*!< Indexing traversal expression. (optional) */
    struct selva_string *filter; /*!< Indexing filter. */
    struct index_order sort; /*!< Sort order of the index. */
};

/**
 * Indexing hint accounting and indices.
 */
struct SelvaFindIndexControlBlock {
    struct {
        /**
         * Permanently created by the user.
         * Permanent ICBs are never destroyed and they are prioritized for
         * indexing.
         */
        unsigned permanent : 1;
        /**
         * Marker id is selected.
         * `marker_id` in this struct is valid only if this flag is set.
         */
        unsigned valid_marked_id : 1;
        /**
         * A timer has been registered and timer_id is valid.
         */
        unsigned valid_timer_id : 1;
        /**
         * Indexing is active.
         * The index `res` set is actively being updated although `res` can be
         * invalid from time to time, meaning that the index is currently
         * invalid.
         */
        unsigned active : 1;
        /**
         * The indexing result `res` is considered valid.
         * This can go 0 even when we are indexing if the `res.set` SelvaSet
         * needs to be refreshed after a `SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY`
         * event was received.
         */
        unsigned valid : 1;
        /**
         * The index is ordered.
         * Use `res.ord` instead of `res.set`.
         */
        unsigned ordered: 1;
    } flags;

    /**
     * Subscription marker updating the index.
     */
    Selva_SubscriptionMarkerId marker_id;

    /**
     * Timer refreshing this index control block.
     */
    int timer_id;

    /**
     * Find result set size accounting.
     */
    struct {
        /**
         * The full search domain size.
         * This is the nubmer of nodes we must traverse to build the find result
         * without indexing.
         */
        float tot_max;
        float tot_max_ave; /*!< Average of `tot_max` over time. */

        /**
         * The number of nodes selected for the find result.
         * This number is updated when the index is not valid and we traversed
         * the hierarchy.
         */
        float take_max;
        float take_max_ave; /*!< Average of `take_max` over time. */

        /**
         * The number of nodes taken from the `res` SelvaSet when the index is valid.
         * This is updated when the index is valid during a find.
         */
        float ind_take_max;
        float ind_take_max_ave; /*!< Average of `ind_take` over time. */
    } find_acc;

    /**
     * Hint popularity counter.
     */
    struct {
        int cur; /*!< Times the hint has been seen during the current period. */
        float ave; /*!< Average times seen over a period of time (FIND_INDEXING_POPULARITY_AVE_PERIOD). */
    } pop_count;

    /**
     * The ICB timer (icb_proc()) needs a reference back to the hierarchy.
     */
    struct SelvaHierarchy *hierarchy;

    /*
     * Traversal.
     */
    Selva_NodeId node_id; /*!< Starting node_id. */
    struct icb_descriptor traversal;

    /**
     * Result set of the indexing clause.
     * Only valid if `flags.valid` is set.
     * The elements in this set are Selva_NodeIds.
     */
    union {
        /**
         * Unordered indexing result.
         */
        struct SelvaSet set;
        /**
         * Ordered indexing result.
         */
        struct SVector ord;
    } res;

    /**
     * Length of name_str.
     */
    size_t name_len;

    /**
     * The name of this ICB in the index for reverse lookup.
     */
    char name_str[0];
};

/**
 * Calculate the length of an index name.
 */
__purefn size_t SelvaFindIndexICB_CalcNameLen(const Selva_NodeId node_id, const struct icb_descriptor *desc);

/**
 * Create a deterministic name for an index.
 * node_id.<direction>[.<dir expression>][.<sort order>.<order field>].H(<indexing clause>)
 * @param buf is a buffer that has at least the length given by SelvaFindIndexICB_CalcNameLen().
 */
void SelvaFindIndexICB_BuildName(char *buf, const Selva_NodeId node_id, const struct icb_descriptor *desc);

/**
 * Get an ICB from the index map.
 */
int SelvaFindIndexICB_Get(struct SelvaHierarchy *hierarchy, const char *name_str, size_t name_len, struct SelvaFindIndexControlBlock **icb);

/**
 * Add an ICB to the index map.
 */
int SelvaFindIndexICB_Set(struct SelvaHierarchy *hierarchy, const char *name_str, size_t name_len, struct SelvaFindIndexControlBlock *icb);

/**
 * Remove an ICB from the index map.
 */
int SelvaFindIndexICB_Del(struct SelvaHierarchy *hierarchy, const struct SelvaFindIndexControlBlock *icb);

#endif /* _FIND_INDEX_ICB_ */
