/*
 * Copyright (c) 2021-2022 SAULX
 * SPDX-License-Identifier: (MIT WITH selva-exception) OR AGPL-3.0-only
 */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <tgmath.h>
#include "redismodule.h"
#include "config.h"
#include "base64.h"
#include "bitmap.h"
#include "lpf.h"
#include "poptop.h"
#include "errors.h"
#include "hierarchy.h"
#include "ida.h"
#include "selva.h"
#include "modinfo.h"
#include "selva_object.h"
#include "selva_onload.h"
#include "selva_set.h"
#include "selva_trace.h"
#include "find_index.h"

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
         * `res` set is considered valid.
         * This can go 0 even when we are indexing if the `res` SelvaSet needs
         * to be refreshed after a `SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY` event
         * was received.
         */
        unsigned valid : 1;
    } flags;

    /**
     * Subscription marker updating the index.
     */
    Selva_SubscriptionMarkerId marker_id;

    /**
     * Timer refreshing this index control block.
     */
    RedisModuleTimerID timer_id;

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
    struct {
        Selva_NodeId node_id; /*!< Starting node_id. */
        enum SelvaTraversal dir; /*!< Traversal direction for the index. */
        union {
            RedisModuleString *dir_field; /*!< Traversal field. */
            RedisModuleString *dir_expression; /*!< Traversal direction rpn expression. */
        };
        RedisModuleString *filter; /*!< Indexing rpn filter. */
    } traversal;

    /**
     * Result set of the indexing clause.
     * Only valid if `flags.valid` is set.
     * The elements in this set are Selva_NodeIds.
     */
    struct SelvaSet res;

    /**
     * Length of name_str.
     */
    size_t name_len;

    /**
     * The name of this ICB in the index for reverse lookup.
     */
    char name_str[0];
};

static float lpf_a; /*!< Popularity count average dampening coefficient. */
Selva_SubscriptionId find_index_sub_id; /* zeroes. */

/*
 * Trace handles.
 */
SELVA_TRACE_HANDLE(FindIndex_AutoIndex);
SELVA_TRACE_HANDLE(FindIndex_icb_proc);
SELVA_TRACE_HANDLE(FindIndex_make_indexing_decission_proc);
SELVA_TRACE_HANDLE(FindIndex_refresh);

static void create_icb_timer(RedisModuleCtx *ctx, struct SelvaFindIndexControlBlock *icb);
static void create_indexing_timer(RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy);

/**
 * Calculate the length of an index name.
 */
static size_t calc_name_len(enum SelvaTraversal dir, size_t dir_expression_len, const Selva_NodeId node_id, size_t filter_len) {
    size_t n;

    n = Selva_NodeIdLen(node_id) + base64_out_len(filter_len, 0) + 3;

    if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        /*
         * Currently only expressions are supported in addition to fixed
         * field name traversals.
         */
        n += base64_out_len(dir_expression_len, 0) + 1;
    }

    return n;
}

/**
 * Create a deterministic name for an index.
 * node_id.<direction>[.<dir expression>].H(<indexing clause>)
 * @param buf is a buffer that has at least the length given by calc_name_len().
 */
static void build_name(
        char *buf,
        const Selva_NodeId node_id,
        enum SelvaTraversal dir, const char *dir_expression_str, size_t dir_expression_len,
        const char *filter_str, size_t filter_len) {
    char *s = buf;

    /* node_id */
    memcpy(s, node_id, Selva_NodeIdLen(node_id));
    s += Selva_NodeIdLen(node_id);

    /* direction */
    *s++ = '.';
    *s++ = 'A' + (char)__builtin_ffs(dir);
    *s++ = '.';

    if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION && dir_expression_len > 0) {
        s += base64_encode_s(s, dir_expression_str, dir_expression_len, 0);
        *s++ = '.';
    }

    /* indexing clause filter */
    s += base64_encode_s(s, filter_str, filter_len, 0);
}

/**
 * Set a new unique marker_id to the given icb.
 */
static int set_marker_id(struct SelvaHierarchy *hierarchy, struct SelvaFindIndexControlBlock *icb) {
    const int next = ida_alloc(hierarchy->dyn_index.ida);

    if (next < 0) {
        return next;
    }

    icb->marker_id = next;
    icb->flags.valid_marked_id = 1;

    return 0;
}

/**
 * Check if this node needs to be skipped.
 * This is functionally equivalent to the skipping happening in the find
 * command, meaning that the resulting index set will look similar to a
 * find result with the same arguments.
 */
static int skip_node(const struct SelvaFindIndexControlBlock *icb, const Selva_NodeId node_id) {
    return SelvaTraversal_GetSkip(icb->traversal.dir) && !memcmp(node_id, icb->traversal.node_id, SELVA_NODE_ID_SIZE);
}

/**
 * A callback function to update the index on hierarchy changes.
 */
static void update_index(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy __unused,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags,
        const struct SelvaHierarchyNode *node) {
    struct SelvaFindIndexControlBlock *icb;

    /*
     * Presumably as long as this function is called the owner_ctx pointer
     * should be always point to a valid icb too.
     */
    icb = (struct SelvaFindIndexControlBlock *)marker->marker_action_owner_ctx;

    if (event_flags & SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY) {
        /* Delete the res to trigger a refresh. */
        if (icb->flags.valid) {
#if 0
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            fprintf(stderr, "%s:%d: The index must be purged and refreshed because %.*s was removed\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id);
#endif

            SelvaSet_Destroy(&icb->res);
            icb->flags.valid = 0;

            /* Clear the accounting. */
            memset(&icb->find_acc, 0, sizeof(icb->find_acc));
        }
    } else if (event_flags & SELVA_SUBSCRIPTION_FLAG_REFRESH) {
        /*
         * Presumably there is no way another command would be handled before
         * we have received an event for every node in the traversal, therefore
         * there is no risk setting this valid before all the ids have been
         * actually added.
         */
        if (!icb->flags.valid) {
            icb->flags.valid = 1;

            /* Initialize the res set before indexing. */
            SelvaSet_Init(&icb->res, SELVA_SET_TYPE_NODEID);
        }

        if (Selva_SubscriptionFilterMatch(ctx, node, marker)) {
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            if (!skip_node(icb, node_id)) {
#if 0
                fprintf(stderr, "%s:%d: Adding node %.*s to the index after refresh\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id);
#endif
                SelvaSet_Add(&icb->res, node_id);
            }
        }
    } else if (event_flags & (SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY | SELVA_SUBSCRIPTION_FLAG_CH_FIELD)) {
        /*
         * Note that SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY applies to both
         * deleting and adding a node. However, we know that currently deleting
         * a node will cause also a SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY event.
         */
        if (icb->flags.valid && Selva_SubscriptionFilterMatch(ctx, node, marker)) {
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            if (!skip_node(icb, node_id)) {
                SelvaSet_Add(&icb->res, node_id);
#if 0
                fprintf(stderr, "%s:%d: Adding node %.*s to the index\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id);
#endif
            }
        }
    } else {
#if 0
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);
        fprintf(stderr, "%s:%d: NOP %x for node %.*s\n",
                __FILE__, __LINE__,
                (unsigned)event_flags, (int)SELVA_NODE_ID_SIZE, node_id);
#endif
    }
}

/**
 * Start an index.
 * Note that refresh_index() needs to be called after
 * this function to actually build the index.
 */
static int start_index(
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    const unsigned short marker_flags =
        SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY |
        SELVA_SUBSCRIPTION_FLAG_CH_FIELD |
        SELVA_SUBSCRIPTION_FLAG_REFRESH;
    const char *dir_field = NULL;
    const char *dir_expression = NULL;
    int err;

    if (icb->traversal.dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        dir_expression = RedisModule_StringPtrLen(icb->traversal.dir_expression, NULL);
    } else if (icb->traversal.dir_field) {
        dir_field = RedisModule_StringPtrLen(icb->traversal.dir_field, NULL);
    }

    err = SelvaSubscriptions_AddCallbackMarker(
            hierarchy, find_index_sub_id, icb->marker_id, marker_flags,
            icb->traversal.node_id, icb->traversal.dir, dir_field, dir_expression, RedisModule_StringPtrLen(icb->traversal.filter, NULL),
            update_index,
            icb);
    if (err) {
        return err;
    }

    icb->flags.active = 1;

    /* Clear indexed find accounting. */
    icb->find_acc.ind_take_max = 0.0f;

    hierarchy->dyn_index.nr_indices++;

    return 0;
}

/**
 * Refresh an existing index created with start_index().
 * @param ctx is a pointer to the current Redis context.
 * @param hierarchy is a pointer to the hierarchy.
 * @param icb is a pointer to the indexed ICB.
 */
static int refresh_index(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    int err;

    if (unlikely(!icb->flags.active)) {
        /*
         * We should really make sure this function will never be called if the
         * index is not active but better be safe than sorry.
         */
        return SELVA_EINVAL;
    }

    SELVA_TRACE_BEGIN(FindIndex_refresh);
    err = SelvaSubscriptions_RefreshByMarkerId(ctx, hierarchy, find_index_sub_id, icb->marker_id);
    SELVA_TRACE_END(FindIndex_refresh);

    return err;
}

/**
 * Destroy-Discard an index.
 */
static int discard_index(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    int err;

    poptop_remove(&hierarchy->dyn_index.top_indices, icb);

    if (hierarchy) {
        /*
         * We assume that if ctx is not given then there are no subscriptions left
         * as we are deleting the hierarchy.
         */
        if (ctx && icb->flags.valid_marked_id) {
            err = SelvaSubscriptions_DeleteMarker(ctx, hierarchy, find_index_sub_id, icb->marker_id);
            if (err && err != SELVA_ENOENT && err != SELVA_SUBSCRIPTIONS_ENOENT) {
                return err;
            }
        }

        if (icb->flags.active) { /* Should be true right? */
            icb->flags.active = 0;
            hierarchy->dyn_index.nr_indices--;
        }
    }

    if (icb->flags.valid) {
        /* Destroy the index but not the control block. */
        icb->flags.valid = 0;
        SelvaSet_Destroy(&icb->res);
    }

    /* Clear accouting. */
    memset(&icb->find_acc, 0, sizeof(icb->find_acc));

    return 0;
}

/**
 * Destroy index control block completely.
 */
__attribute__((nonnull (2, 3))) static int destroy_icb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    int err;

#if 0
    fprintf(stderr, "%s:%d: Destroying icb for %.*s\n",
            __FILE__, __LINE__,
            (int)icb->name_len, icb->name_str);
#endif

    err = discard_index(ctx, hierarchy, icb);
    if (err) {
        fprintf(stderr, "%s:%d: Failed to discard an index for \"%.*s\": %s\n",
                __FILE__, __LINE__,
                (int)icb->name_len, icb->name_str,
                getSelvaErrorStr(err));
        return err;
    }

    if (hierarchy->dyn_index.index_map) {
        err = SelvaObject_DelKeyStr(hierarchy->dyn_index.index_map, icb->name_str, icb->name_len);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to destroy an index for \"%.*s\": %s\n",
                    __FILE__, __LINE__,
                    (int)icb->name_len, icb->name_str,
                    getSelvaErrorStr(err));
            return err;
        }
    }

    if (icb->flags.valid_marked_id) {
        ida_free(hierarchy->dyn_index.ida, icb->marker_id);
    }

    /*
     * Note that we shouldn't need to pass ctx to RedisModule_FreeString() as the
     * strings were probably retained long time ago. It's safest to pass it anyway
     * in case we'd end up here within the same context where the strings were
     * passed to it. The API doc says we should always pass ctx if the string was
     * created with one but that doesn't seem to be exactly true. Therefore we allow
     * ctx to be NULL.
     */
    if (icb->traversal.dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION && icb->traversal.dir_expression) {
        RedisModule_FreeString(ctx, icb->traversal.dir_expression);
    } else if (icb->traversal.dir_field) {
        /* Practically not used at the moment. */
        RedisModule_FreeString(ctx, icb->traversal.dir_field);
    }

    if (icb->traversal.filter) {
        RedisModule_FreeString(ctx, icb->traversal.filter);
    }

    if (icb->flags.valid_timer_id) {
        RedisModule_StopTimerUnsafe(icb->timer_id, NULL);
    }

    memset(icb, 0, sizeof(*icb));
    RedisModule_Free(icb);

    return 0;
}

/**
 * Indexing timer handler.
 * Only one instance is registered per hierarchy key.
 * This function maintains the poptop list of indices and decides for each ICB on the
 * list to either
 * 1) discard its index,
 * 2) discard its and destroy the ICB, or
 * 3) create an index; if one didn't exist yet and
 *    the max number of indices hasn't been exceeded yet.
 */
static void make_indexing_decission_proc(RedisModuleCtx *ctx, void *data) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_make_indexing_decission_proc);
    SelvaHierarchy *hierarchy = (struct SelvaHierarchy *)data;
    struct poptop_list_el *el;

#if 0
    fprintf(stderr, "TOCK\n");
#endif

    hierarchy->dyn_index.proc_timer_active = 0;
    create_indexing_timer(ctx, hierarchy);

    /*
     * First run the maintenance to determine the new cutoff limit and
     * whether current indices will require actions.
     */
    if (poptop_maintenance(&hierarchy->dyn_index.top_indices)) {
        struct SelvaFindIndexControlBlock *icb;

        /*
         * First discard indices that are no longer relevant.
         */
        while ((icb = poptop_maintenance_drop(&hierarchy->dyn_index.top_indices))) {
            if (icb->flags.permanent || icb->pop_count.ave > 0.1f) { /* TODO How to determine the pop_count ave discard limit? */
                /*
                 * Discard the index.
                 * The hint has appeared in find queries in the recent past but
                 * the poptop score is falling under the drop limit, therefore
                 * we'll keep the ICB but discard the index.
                 * Even if the ICB was marked as permanent we still must discard
                 * the index for now because it's not safe to call
                 * poptop_maybe_add() here.
                 */
                int err;

                fprintf(stderr, "%s:%d: Discarding index for \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)icb->name_len, icb->name_str);

                err = discard_index(ctx, hierarchy, icb);
                if (err) {
                    fprintf(stderr, "%s:%d: Failed to discard the index for \"%.*s\": %s\n",
                            __FILE__, __LINE__,
                            (int)icb->name_len, icb->name_str,
                            getSelvaErrorStr(err));
                }
            } else {
                /*
                 * Destroy the index.
                 * The hint hasn't been seen for a some time and we are going to
                 * discard the index and destroy the ICB.
                 */
                int err;

                fprintf(stderr, "%s:%d: Destroying index for \"%.*s\"\n",
                        __FILE__, __LINE__,
                        (int)icb->name_len, icb->name_str);

                err = destroy_icb(ctx, hierarchy, icb);
                if (err) {
                    fprintf(stderr, "%s:%d: Failed to destroy the index for \"%.*s\": %s\n",
                            __FILE__, __LINE__,
                            (int)icb->name_len, icb->name_str,
                            getSelvaErrorStr(err));
                }
            }
        }
    }

    /*
     * Finally make sure the top most ICBs are indexed.
     */
    POPTOP_FOREACH(el, &hierarchy->dyn_index.top_indices) {
        struct SelvaFindIndexControlBlock *icb = (struct SelvaFindIndexControlBlock *)el->p;
        int err;

        /* POPTOP_FOREACH can return NULLs. */
        if (!icb) {
            continue;
        }

        if (icb->flags.valid) {
            continue; /* Index already created and valid. */
        }

        /*
         * The index is not yet active but this ICB should be indexed.
         */
        if (!icb->flags.active) {
            /*
             * The maximum number of indices has been reached but we may still
             * need to refresh other indices.
             */
            if (hierarchy->dyn_index.nr_indices >= selva_glob_config.find_indices_max) {
                continue;
            }

            /*
             * Start indexing this ICB.
             */
            err = start_index(hierarchy, icb);
            if (err) {
                fprintf(stderr, "%s:%d: Failed to create an index for \"%.*s\": %s\n",
                        __FILE__, __LINE__,
                        (int)icb->name_len, icb->name_str,
                        getSelvaErrorStr(err));
            } else {
                fprintf(stderr, "%s:%d: Created an index for \"%.*s\"\n",
                       __FILE__, __LINE__,
                      (int)icb->name_len, icb->name_str);
            }
        }

        /*
         * Since we are using the SELVA_SUBSCRIPTION_FLAG_REFRESH flag the call to
         * refresh will call the action function for each node and thus build the
         * initial index.
         */
        err = refresh_index(ctx, hierarchy, icb);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to refresh the index for \"%.*s\": %s\n",
                    __FILE__, __LINE__,
                    (int)icb->name_len, icb->name_str,
                    getSelvaErrorStr(err));

            /*
             * Destroy the ICB because it's likely that creating this index would
             * fail on every further attempt.
             * This should be relatively safe as the removal won't change the
             * ordering of top_indices.
             */
            err = destroy_icb(ctx, hierarchy, icb);
            if (err) {
                fprintf(stderr, "%s:%d: Failed to destroy the index for \"%.*s\": %s\n",
                        __FILE__, __LINE__,
                        (int)icb->name_len, icb->name_str,
                        getSelvaErrorStr(err));
            }
        }
    }
}

/**
 * Calculate new score for a given ICB to be used with the top_indices poptop.
 */
static float calc_icb_score(const struct SelvaFindIndexControlBlock *icb) {
    float pop_count = icb->pop_count.ave;
    float res_set_size;
    float tot_max;

    if (icb->flags.valid) {
        res_set_size = icb->find_acc.ind_take_max_ave;
        tot_max = max(icb->find_acc.tot_max_ave, (float)SelvaSet_Size(&icb->res));
    } else {
        res_set_size = icb->find_acc.take_max_ave;
        tot_max = icb->find_acc.tot_max_ave;
    }

#if 0
    return pow(1.0f - res_set_size / tot_max, 1.0f / 3.0f) * pop_count;
#endif
    return (1.0f - res_set_size / tot_max) * pop_count;
}

/**
 * Per ICB timer handler.
 * This function collects, calculates the information needed to make indexing
 * decissions, and makes the preliminary decission to include the ICB in the
 * actual indexing decission process by inserting the ICB into the list of
 * ICBs considered for indexing.
 */
static void icb_proc(RedisModuleCtx *ctx, void *data) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_icb_proc);
    struct SelvaFindIndexControlBlock *icb = (struct SelvaFindIndexControlBlock *)data;

#if 0
    fprintf(stderr, "TICK\n");
#endif

    /* Recreate the timer. */
    icb->flags.valid_timer_id = 0;
    create_icb_timer(ctx, icb);

    /*
     * Calculate the average popularity of the associated hint.
     */
    icb->pop_count.ave = lpf_calc_next(lpf_a, icb->pop_count.ave, (float)icb->pop_count.cur);
    icb->pop_count.cur = 0; /* Reset the counting to start the next period. */

    /*
     * Update the find result set size accounting averages.
     */
    if (icb->flags.valid) {
        icb->find_acc.ind_take_max_ave = lpf_calc_next(lpf_a, icb->find_acc.ind_take_max_ave, icb->find_acc.ind_take_max);
    } else {
        icb->find_acc.tot_max_ave = lpf_calc_next(lpf_a, icb->find_acc.tot_max_ave, icb->find_acc.tot_max);
        icb->find_acc.take_max_ave = lpf_calc_next(lpf_a, icb->find_acc.take_max_ave, icb->find_acc.take_max);
    }
    icb->find_acc.tot_max = 0.0f;
    icb->find_acc.take_max = 0.0f;
    icb->find_acc.ind_take_max = 0.0f;

    /*
     * Consider the index hint for indexing if take exceeds the threshold.
     */
    if (icb->flags.active || icb->flags.permanent ||
        icb->find_acc.tot_max_ave >= (float)selva_glob_config.find_indexing_threshold) {
        float score;

        if (icb->flags.permanent) {
            /*
             * Try to make sure a permanent index stays in the poptop list.
             * But also try not to spoil the median.
             */
            score = icb->hierarchy->dyn_index.top_indices.cut_limit * 1.1f;
        } else {
            score = calc_icb_score(icb);
        }

        /*
         * Insert the icb into the poptop list, maybe, and it might get indexed
         * in the near future. However, the insertion might not happen if score
         * doesn't exceed the cut limit determined by poptop.
         */
        poptop_maybe_add(&icb->hierarchy->dyn_index.top_indices, score, icb);
#if 0
        fprintf(stderr, "%s:%d: Maybe added %.*s:%p to poptop with score: %f\n",
                __FILE__, __LINE__,
                (int)icb->name_len, icb->name_str,
                icb,
                score);
#endif
    }
}

/**
 * Get or create an indexing control block.
 */
static struct SelvaFindIndexControlBlock *upsert_icb(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        const Selva_NodeId node_id,
        enum SelvaTraversal dir,
        RedisModuleString *dir_expression,
        RedisModuleString *filter) {
    struct SelvaObject *dyn_index = hierarchy->dyn_index.index_map;
    TO_STR(dir_expression);
    TO_STR(filter);
    void *p;
    struct SelvaFindIndexControlBlock *icb;
    int err;

    /*
     * Get a deterministic name for indexing this find query.
     */
    const size_t name_len = calc_name_len(dir, dir_expression_len, node_id, filter_len);
    char name_str[name_len];

    build_name(name_str, node_id, dir, dir_expression_str, dir_expression_len, filter_str, filter_len);

    err = SelvaObject_GetPointerStr(dyn_index, name_str, name_len, &p);
    icb = p;
    if (err) {
        /*
         * ICB not found, so we create one.
         */
        if (err != SELVA_ENOENT) {
            fprintf(stderr, "%s:%d: Get ICB for \"%.*s\" failed: %s\n",
                    __FILE__, __LINE__,
                    (int)name_len, name_str,
                    getSelvaErrorStr(err));
            return NULL;
        }

        /*
         * Create it.
         * This doesn't mean that we are necessarily going to create the index
         * yet but we are going to start counting wether it makes sense to start
         * indexing a query described by the arguments of this function.
         */
        icb = RedisModule_Calloc(1, sizeof(*icb) + name_len);
        if (!icb) {
            return NULL;
        }

        icb->name_len = name_len;
        memcpy(icb->name_str, name_str, name_len);

        icb->hierarchy = hierarchy;

        err = set_marker_id(hierarchy, icb);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to get a new marker id for an index \"%.*s\": %s\n",
                    __FILE__, __LINE__,
                    (int)icb->name_len, icb->name_str,
                    getSelvaErrorStr(err));

            destroy_icb(ctx, hierarchy, icb);
            return NULL;
        }

        /*
         * Initialize the popularity count average.
         * We give it a fairly large initial value for the count to make sure
         * the ICB will stay active for some time but doesn't necessarily get
         * turned into an index immediately.
         * The multiplier here could be a tunable but I'm not sure if anybody
         * wants to really tune it.
         */
        icb->pop_count.ave = 2.0f * (float)(selva_glob_config.find_indexing_interval / selva_glob_config.find_indexing_icb_update_interval) * (1.0f / lpf_a);

        /*
         * Set traversal params.
         */
        memcpy(icb->traversal.node_id, node_id, SELVA_NODE_ID_SIZE);
        icb->traversal.dir = dir;

        /* TODO Can it ever be dir_field, currently no... */
        if (dir_expression) {
            RedisModule_RetainString(ctx, dir_expression);
        }
        icb->traversal.dir_expression = dir_expression;
        RedisModule_RetainString(ctx, filter);
        icb->traversal.filter = filter;

        /*
         * Map the newly created icb into the dyn_index SelvaObject.
         */
        err = SelvaObject_SetPointerStr(dyn_index, name_str, name_len, icb, NULL);
        if (err) {
            fprintf(stderr, "%s:%d: Failed to insert a new ICB at \"%.*s\": %s\n",
                    __FILE__, __LINE__,
                    (int)name_len, name_str,
                    getSelvaErrorStr(err));

            destroy_icb(ctx, hierarchy, icb);
            return NULL;
        }

        /* Finally create a proc timer. */
        create_icb_timer(ctx, icb);

    }

    return icb;
}

/**
 * Create a timer for an ICB.
 * Generally every ICB has a timer.
 */
static void create_icb_timer(RedisModuleCtx *ctx, struct SelvaFindIndexControlBlock *icb) {
    const mstime_t period = selva_glob_config.find_indexing_icb_update_interval;

    assert(icb->flags.valid_timer_id == 0);
    icb->timer_id = RedisModule_CreateTimer(ctx, period, icb_proc, icb);
    icb->flags.valid_timer_id = 1;
}

/**
 * Create a timer for indexing the hierarchy.
 * Only one timer per hierarchy should be created.
 */
static void create_indexing_timer(RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy) {
    const mstime_t period = selva_glob_config.find_indexing_interval;

    assert(hierarchy->dyn_index.proc_timer_active == 0);
    hierarchy->dyn_index.proc_timer_id = RedisModule_CreateTimer(ctx, period, make_indexing_decission_proc, hierarchy);
    hierarchy->dyn_index.proc_timer_active = 1;
}

int SelvaFindIndex_Init(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy) {
    if (selva_glob_config.find_indices_max == 0) {
        return 0; /* Indexing disabled. */
    }

    hierarchy->dyn_index.index_map = SelvaObject_New();
    if (!hierarchy->dyn_index.index_map) {
        goto fail;
    }

    hierarchy->dyn_index.ida = ida_init(FIND_INDICES_MAX_HINTS);
    if (!hierarchy->dyn_index.ida) {
        goto fail;
    }

    /*
     * We allow a max of 2 * FIND_INDICES_MAX so we always have more to choose
     * from than we'll be able to create, and knowing that poptop will
     * periodically drop about half of the list, we want to avoid oscillating
     * too much.
     */
    poptop_init(&hierarchy->dyn_index.top_indices, 2 * selva_glob_config.find_indices_max, 0.0f);

    create_indexing_timer(ctx, hierarchy);

    return 0;
fail:
    SelvaObject_Destroy(hierarchy->dyn_index.index_map);
    ida_destroy(hierarchy->dyn_index.ida);
    return SELVA_ENOMEM;
}

/* TODO Could possibly use the built-in free */
static void deinit_index_obj(struct SelvaHierarchy *hierarchy, struct SelvaObject *obj) {
    SelvaObject_Iterator *it;
    enum SelvaObjectType type;
    void *p;

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, NULL, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            (void)destroy_icb(NULL, hierarchy, (struct SelvaFindIndexControlBlock *)p);
        } else if (type == SELVA_OBJECT_OBJECT) {
            deinit_index_obj(hierarchy, (struct SelvaObject *)p);
        }
    }
}

void SelvaFindIndex_Deinit(struct SelvaHierarchy *hierarchy) {
    if (selva_glob_config.find_indices_max == 0) {
        return; /* Indexing disabled. */
    }

    if (hierarchy->dyn_index.index_map) {
        deinit_index_obj(hierarchy, hierarchy->dyn_index.index_map);
        SelvaObject_Destroy(hierarchy->dyn_index.index_map);
    }
    poptop_deinit(&hierarchy->dyn_index.top_indices);
    ida_destroy(hierarchy->dyn_index.ida);

    if (hierarchy->dyn_index.proc_timer_active) {
        RedisModule_StopTimerUnsafe(hierarchy->dyn_index.proc_timer_id, NULL);
    }

    memset(&hierarchy->dyn_index, 0, sizeof(hierarchy->dyn_index));
}

int SelvaFind_AutoIndex(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, RedisModuleString *dir_expression,
        const Selva_NodeId node_id,
        RedisModuleString *filter,
        struct SelvaFindIndexControlBlock **icb_out,
        struct SelvaSet **out) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_AutoIndex);
    struct SelvaFindIndexControlBlock *icb;
    TO_STR(filter);
    filter_str;

    if (!filter || filter_len == 0 || selva_glob_config.find_indices_max == 0) {
        return SELVA_EINVAL;
    }

    /*
     * Only index some traversals.
     */
    if (!(dir & (
                 SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS |
                 SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS |
                 SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION
                ))) {
        return 0;
    }

    icb = upsert_icb(ctx, hierarchy, node_id, dir, dir_expression, filter);
    *icb_out = icb;

    if (!icb || !icb->flags.valid) {
        return SELVA_ENOENT;
    }

    *out = &icb->res;
    return 0;
}

void SelvaFind_Acc(struct SelvaFindIndexControlBlock * restrict icb, size_t acc_take, size_t acc_tot) {
    if (selva_glob_config.find_indices_max == 0) {
        /* If indexing is disabled then the rest of the function will be optimized out. */
        return;
    }

    /* Increment popularity counter. */
    if (icb->pop_count.cur < INT_MAX) {
        icb->pop_count.cur++;
    }

    /*
     * Result set size accounting.
     */
    if (icb->flags.valid) {
        if ((float)acc_take > icb->find_acc.ind_take_max) {
            icb->find_acc.ind_take_max = (float)acc_take;
        }
    } else {
        if ((float)acc_take > icb->find_acc.take_max || (float)acc_tot > icb->find_acc.tot_max) {
            icb->find_acc.take_max = (float)acc_take;
            icb->find_acc.tot_max = (float)acc_tot;
        }
    }
}

/* TODO Could use the built-in reply functionality. */
static int list_index(RedisModuleCtx *ctx, struct SelvaObject *obj) {
    SelvaObject_Iterator *it;
    enum SelvaObjectType type;
    void *p;
    int n = 0;

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, NULL, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            const struct SelvaFindIndexControlBlock *icb = (const struct SelvaFindIndexControlBlock *)p;

            /*
             * index_name, [ take, total, ind_take, ind_size ]
             */
            n++;
            RedisModule_ReplyWithStringBuffer(ctx, icb->name_str, icb->name_len);
            RedisModule_ReplyWithArray(ctx, 4);
            RedisModule_ReplyWithDouble(ctx, (double)icb->find_acc.take_max_ave);
            RedisModule_ReplyWithDouble(ctx, (double)icb->find_acc.tot_max_ave);
            RedisModule_ReplyWithDouble(ctx, (double)icb->find_acc.ind_take_max_ave);
            if (!icb->flags.active) {
                RedisModule_ReplyWithSimpleString(ctx, "not_active");
            } else if (!icb->flags.valid) {
                RedisModule_ReplyWithSimpleString(ctx, "not_valid");
            } else {
                RedisModule_ReplyWithDouble(ctx, (double)SelvaSet_Size(&icb->res));
            }
        } else if (type == SELVA_OBJECT_OBJECT) {
            n += list_index(ctx, (struct SelvaObject *)p);
        } else {
            fprintf(stderr, "%s:%d: Unsupported index type: %s\n",
                    __FILE__, __LINE__,
                    SelvaObject_Type2String(type, NULL));
        }
    }

    return n;
}

static int SelvaFindIndex_ListCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    SelvaHierarchy *hierarchy;

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }
    if (!hierarchy->dyn_index.index_map) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    RedisModule_ReplySetArrayLength(ctx, 2 * list_index(ctx, hierarchy->dyn_index.index_map));

    return REDISMODULE_OK;
}

static int SelvaFindIndex_NewCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    SelvaHierarchy *hierarchy;
    enum SelvaTraversal dir;
    RedisModuleString *dir_expression;
    const RedisModuleString *rms_node_id;
    Selva_NodeId node_id;
    RedisModuleString *filter;
    struct SelvaFindIndexControlBlock *icb = NULL;
    struct SelvaSet *set;
    int err;

    if (argc != 6) {
        return RedisModule_WrongArity(ctx);
    }

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_DIRECTION = 2;
    const int ARGV_REF_FIELD = 3;
    const int ARGV_NODE_ID = 4;
    const int ARGV_FILTER = 5;

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }

    if (!hierarchy->dyn_index.index_map) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Traversal argument");
    }

    if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        dir_expression = argv[ARGV_REF_FIELD];
    } else {
        dir_expression = NULL;
    }

    rms_node_id = argv[ARGV_NODE_ID];
    TO_STR(rms_node_id);
    if (rms_node_id_len <= SELVA_NODE_TYPE_SIZE || rms_node_id_len > SELVA_NODE_ID_SIZE) {
        return replyWithSelvaErrorf(ctx, SELVA_EINVAL, "node_id");
    }
    Selva_NodeIdCpy(node_id, rms_node_id_str);

    /* TODO Validate */
    filter = argv[ARGV_FILTER];

    err = SelvaFind_AutoIndex(ctx, hierarchy, dir, dir_expression, node_id, filter, &icb, &set);
    if ((err && err != SELVA_ENOENT) || !icb) {
        return replyWithSelvaErrorf(ctx, err, "Failed to create an index");
    }

    /*
     * Make sure that an index will be created.
     * The first three parameters will make sure we have a coefficient of 1.
     * The last two are a lazy attempt to get this ICB on the top of the list.
     */
    float v = (float)selva_glob_config.find_indexing_threshold + 1.0f;
    icb->find_acc.tot_max = v;
    icb->find_acc.tot_max_ave = v;
    icb->find_acc.take_max = v;
    icb->find_acc.take_max_ave = v;
    icb->find_acc.ind_take_max = v;
    icb->find_acc.ind_take_max_ave = v;
    icb->pop_count.cur = 1000.0f;
    icb->pop_count.ave = 1000.0f;
    icb->flags.permanent = 1;

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static int SelvaFindIndex_DelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    SelvaHierarchy *hierarchy;
    void *p;
    struct SelvaFindIndexControlBlock *icb;
    int discard = 0;
    int err;

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_INDEX = 2;
    const int ARGV_OP = 3;

    if (argc == 4) {
        const RedisModuleString *op = argv[ARGV_OP];
        TO_STR(op);

        if (op_len == 1 && op_str[0] == '0') {
            discard = 0;
        } else if (op_len == 1 && op_str[0] == '1') {
            discard = 1;
        } else {
            return replyWithSelvaError(ctx, SELVA_ENOTSUP);
        }
    } else if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send redis messages here. */
        return REDISMODULE_OK;
    }
    if (!hierarchy->dyn_index.index_map) {
        return replyWithSelvaError(ctx, SELVA_ENOENT);
    }

    err = SelvaObject_GetPointer(hierarchy->dyn_index.index_map, argv[ARGV_INDEX], &p);
    icb = p;
    if (err == SELVA_ENOENT) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    } else if (err) {
        return replyWithSelvaError(ctx, err);
    }

    if (discard) {
        err = discard_index(ctx, hierarchy, icb);
    } else {
        err = destroy_icb(ctx, hierarchy, icb);
    }
    if (err) {
        return replyWithSelvaError(ctx, err);
    }

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static void mod_info(RedisModuleInfoCtx *ctx) {
    (void)RedisModule_InfoAddFieldDouble(ctx, "lpf_a", lpf_a);
}
SELVA_MODINFO("find_index", mod_info);

static int FindIndex_OnLoad(RedisModuleCtx *ctx) {
    lpf_a = lpf_geta((float)selva_glob_config.find_indexing_popularity_ave_period, (float)selva_glob_config.find_indexing_icb_update_interval / 1000.0f);

    if (RedisModule_CreateCommand(ctx, "selva.index.list", SelvaFindIndex_ListCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.index.new", SelvaFindIndex_NewCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR ||
        RedisModule_CreateCommand(ctx, "selva.index.del", SelvaFindIndex_DelCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
SELVA_ONLOAD(FindIndex_OnLoad);
