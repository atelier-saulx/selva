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
#include "jemalloc.h"
#include "config.h"
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
#include "icb.h"
#include "pick_icb.h"
#include "find_index.h"

#define INDEX_ERR_MSG_DISABLED "Indexing disabled"

static float lpf_a; /*!< Popularity count average dampening coefficient. */
Selva_SubscriptionId find_index_sub_id; /* zeroes. */

static const enum SelvaTraversal allowed_dirs =
    SELVA_HIERARCHY_TRAVERSAL_BFS_ANCESTORS |
    SELVA_HIERARCHY_TRAVERSAL_BFS_DESCENDANTS |
    SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION;


/*
 * Trace handles.
 */
SELVA_TRACE_HANDLE(FindIndex_AutoIndex);
SELVA_TRACE_HANDLE(FindIndex_icb_proc);
SELVA_TRACE_HANDLE(FindIndex_make_indexing_decission_proc);
SELVA_TRACE_HANDLE(FindIndex_refresh);

static void create_icb_timer(RedisModuleCtx *ctx, struct SelvaFindIndexControlBlock *icb);
static void create_indexing_timer(RedisModuleCtx *ctx, struct SelvaHierarchy *hierarchy);

static int is_indexing_active(const struct SelvaHierarchy *hierarchy) {
    return !!hierarchy->dyn_index.index_map;
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
static int skip_node(const struct SelvaFindIndexControlBlock *icb, const struct SelvaHierarchyNode *node) {
    Selva_NodeId node_id;

    SelvaHierarchy_GetNodeId(node_id, node);
    return SelvaTraversal_GetSkip(icb->traversal.dir) && !memcmp(node_id, icb->traversal.node_id, SELVA_NODE_ID_SIZE);
}

static void icb_res_init(struct SelvaFindIndexControlBlock *icb) {
    if (icb->flags.ordered) {
        const size_t initial_len = (size_t)icb->find_acc.take_max_ave;

        SelvaTraversalOrder_InitOrderResult(&icb->res.ord, icb->sort.order, initial_len);
    } else {
        SelvaSet_Init(&icb->res.set, SELVA_SET_TYPE_NODEID);
    }
}

static void icb_clear_acc(struct SelvaFindIndexControlBlock *icb) {
    memset(&icb->find_acc, 0, sizeof(icb->find_acc));
}

static void icb_res_destroy(struct SelvaFindIndexControlBlock *icb) {
    if (icb->flags.valid) {
        icb->flags.valid = 0;

        if (icb->flags.ordered) {
            /* ctx is not needed here as it was not used when the items were created. */
            SelvaTraversalOrder_DestroyOrderResult(NULL, &icb->res.ord);
        } else {
            SelvaSet_Destroy(&icb->res.set);
        }
    }
}

static int icb_res_add(struct SelvaFindIndexControlBlock *icb, struct SelvaHierarchyNode *node) {
    if (icb->flags.ordered) {
        struct TraversalOrderItem *item;
        /*
         * Supporting lang here wouldn't add anything because we'd need to index
         * each lang separately anyway.
         */
        RedisModuleString *lang = NULL;

        item = SelvaTraversalOrder_CreateNodeOrderItem(NULL, lang, node, icb->sort.order_field);
        if (SVector_InsertFast(&icb->res.ord, item)) {
            SelvaTraversalOrder_DestroyOrderItem(NULL, item);
        }
    } else {
        Selva_NodeId node_id;
        int err;

        SelvaHierarchy_GetNodeId(node_id, node);

        err = SelvaSet_Add(&icb->res.set, node_id);
        if (err && err != SELVA_EEXIST) {
            return err;
        }
    }

    return 0;
}

size_t SelvaFindIndex_IcbCard(const struct SelvaFindIndexControlBlock *icb) {
    if (icb->flags.ordered) {
        return SVector_Size(&icb->res.ord);
    } else {
        return SelvaSet_Size(&icb->res.set);
    }
}

/**
 * A callback function to update the index on hierarchy changes.
 */
static void update_index(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy __unused,
        struct Selva_SubscriptionMarker *marker,
        unsigned short event_flags,
        const char *field_str,
        size_t field_len,
        struct SelvaHierarchyNode *node) {
    struct SelvaFindIndexControlBlock *icb;

    /*
     * Presumably as long as this function is called the owner_ctx pointer
     * should be always point to a valid icb too.
     */
    icb = (struct SelvaFindIndexControlBlock *)marker->marker_action_owner_ctx;

    if (event_flags & SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY) {
        /*
         * A node within the index was deleted.
         *
         * Delete the res to trigger a full refresh.
         */
        if (icb->flags.valid) {
#if 0
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            fprintf(stderr, "%s:%d: The index must be purged and refreshed because %.*s was removed\n",
                    __FILE__, __LINE__,
                    (int)SELVA_NODE_ID_SIZE, node_id);
#endif

            icb_res_destroy(icb);
            icb_clear_acc(icb); /* Clear to avoid stale data. */
        }
    } else if (event_flags & SELVA_SUBSCRIPTION_FLAG_REFRESH) {
        /*
         * Rebuild the index.
         *
         * The subscription for this index is refreshing and we'll be getting
         * called with SELVA_SUBSCRIPTION_FLAG_REFRESH once for each node in the
         * traversal.
         */
        if (!icb->flags.valid) {
            /*
             * Presumably there is no way another command would be handled before
             * we have received an event for every node in the traversal,
             * therefore there is no risk setting this result valid before all
             * the ids have been actually added.
             */
            icb->flags.valid = 1;

            /* Initialize `res` before indexing. */
            icb_res_init(icb);
        }

        if (Selva_SubscriptionFilterMatch(ctx, hierarchy, node, marker)) {
            if (!skip_node(icb, node)) {
#if 0
                Selva_NodeId node_id;

                SelvaHierarchy_GetNodeId(node_id, node);
                fprintf(stderr, "%s:%d: Adding node %.*s to the index after refresh\n",
                        __FILE__, __LINE__,
                        (int)SELVA_NODE_ID_SIZE, node_id);
#endif
                icb_res_add(icb, node);
            }
        }
    } else if (event_flags & (SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY | SELVA_SUBSCRIPTION_FLAG_CH_FIELD)) {
        /*
         * An additive change in the hierarchy.
         *
         * Note that SELVA_SUBSCRIPTION_FLAG_CH_HIERARCHY applies to both
         * deleting and adding a node. However, we know that currently deleting
         * a node will cause also a SELVA_SUBSCRIPTION_FLAG_CL_HIERARCHY event.
         */
        if (icb->flags.valid && !skip_node(icb, node)) {
            size_t order_field_len;
            const char *order_field_str = RedisModule_StringPtrLen(icb->sort.order_field, &order_field_len);

            if (icb->flags.ordered &&
                (event_flags & SELVA_SUBSCRIPTION_FLAG_CH_FIELD) &&
                field_len == order_field_len && !memcmp(field_str, order_field_str, order_field_len)) {
                /*
                 * Ordered indexing can't execute an additive change when the
                 * field changed is the same as the field used for ordering
                 * (`order_field`). If the value of `order_field` would change
                 * it would cause the same node to occur twice or more in `res`.
                 * Therefore we need to start over.
                 */
                icb_res_destroy(icb);
            } else if (Selva_SubscriptionFilterMatch(ctx, hierarchy, node, marker)) {
                icb_res_add(icb, node);
#if 0
                Selva_NodeId node_id;

                SelvaHierarchy_GetNodeId(node_id, node);
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

    /* Destroy the index but not the control block. */
    icb_res_destroy(icb);
    icb_clear_acc(icb);

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
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to discard an index for \"%.*s\": %s\n",
                  (int)icb->name_len, icb->name_str,
                  getSelvaErrorStr(err));
        return err;
    }

    err = SelvaFindIndexICB_Del(hierarchy, icb);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy an index for \"%.*s\": %s\n",
                  (int)icb->name_len, icb->name_str,
                  getSelvaErrorStr(err));
        return err;
    }

    if (icb->flags.valid_marked_id) {
        ida_free(hierarchy->dyn_index.ida, (ida_t)icb->marker_id);
    }

    /*
     * Note that we shouldn't need to pass ctx to RedisModule_FreeString() as the
     * strings were probably retained long time ago. It's safest to pass it anyway
     * in case we'd end up here within the same context where the strings were
     * passed to it. The API doc says we should always pass ctx if the string was
     * created with one but that doesn't seem to be exactly true. Therefore we allow
     * ctx to be NULL.
     */
    if (icb->traversal.dir_expression) {
        RedisModule_FreeString(ctx, icb->traversal.dir_expression);
    }

    if (icb->traversal.filter) {
        RedisModule_FreeString(ctx, icb->traversal.filter);
    }

    if (icb->flags.valid_timer_id) {
        RedisModule_StopTimerUnsafe(icb->timer_id, NULL);
    }

    memset(icb, 0, sizeof(*icb));
    selva_free(icb);

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

                SELVA_LOG(SELVA_LOGL_INFO, "Discarding index for \"%.*s\"\n",
                          (int)icb->name_len, icb->name_str);

                err = discard_index(ctx, hierarchy, icb);
                if (err) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to discard the index for \"%.*s\": %s\n",
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

                SELVA_LOG(SELVA_LOGL_INFO, "Destroying index for \"%.*s\"\n",
                          (int)icb->name_len, icb->name_str);

                err = destroy_icb(ctx, hierarchy, icb);
                if (err) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy the index for \"%.*s\": %s\n",
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
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an index for \"%.*s\": %s\n",
                          (int)icb->name_len, icb->name_str,
                          getSelvaErrorStr(err));
            } else {
                SELVA_LOG(SELVA_LOGL_INFO, "Created an index for \"%.*s\"\n",
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
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to refresh the index for \"%.*s\": %s\n",
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
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy the index for \"%.*s\": %s\n",
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
    const float pop_count = icb->pop_count.ave;
    const float take = (icb->flags.valid) ? icb->find_acc.ind_take_max_ave : icb->find_acc.take_max_ave;

    return pop_count * take;
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
        struct icb_descriptor *desc) {
    struct SelvaFindIndexControlBlock *icb;
    int err;

    /*
     * Get a deterministic name for indexing this find query.
     */
    const size_t name_len = SelvaFindIndexICB_CalcNameLen(node_id, desc);
    char name_str[name_len];

    SelvaFindIndexICB_BuildName(name_str, node_id, desc);

    err = SelvaFindIndexICB_Get(hierarchy, name_str, name_len, &icb);
    if (err == SELVA_ENOENT) {
        /*
         * ICB not found, so we create one.
         *
         * This doesn't mean that we are necessarily going to create the index
         * yet but we are going to start counting wether it makes sense to start
         * indexing a query described by the arguments of this function.
         */
        icb = selva_calloc(1, sizeof(*icb) + name_len);
        icb->name_len = name_len;
        memcpy(icb->name_str, name_str, name_len);

        icb->hierarchy = hierarchy;

        err = set_marker_id(hierarchy, icb);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to get a new marker id for an index \"%.*s\": %s\n",
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
        icb->traversal.dir = desc->dir;

        /* Note that dir_field is not supported. */
        if (desc->dir_expression) {
            RedisModule_RetainString(ctx, desc->dir_expression);
        }
        icb->traversal.dir_expression = desc->dir_expression;
        RedisModule_RetainString(ctx, desc->filter);
        icb->traversal.filter = desc->filter;

        /* Order */
        icb->sort.order = desc->sort.order;
        if (desc->sort.order != SELVA_RESULT_ORDER_NONE) {
            icb->flags.ordered = 1;
            RedisModule_RetainString(ctx, desc->sort.order_field);
            icb->sort.order_field = desc->sort.order_field;
        }

        /*
         * Map the newly created icb into the dyn_index SelvaObject.
         */
        err = SelvaFindIndexICB_Set(hierarchy, name_str, name_len, icb);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to insert a new ICB at \"%.*s\": %s\n",
                      (int)name_len, name_str,
                      getSelvaErrorStr(err));

            destroy_icb(ctx, hierarchy, icb);
            return NULL;
        }

        /* Finally create a proc timer. */
        create_icb_timer(ctx, icb);
    } else if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Get ICB for \"%.*s\" failed: %s\n",
                  (int)name_len, name_str,
                  getSelvaErrorStr(err));

        icb = NULL;
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

void SelvaFindIndex_Init(RedisModuleCtx *ctx, SelvaHierarchy *hierarchy) {
    if (selva_glob_config.find_indices_max == 0) {
        return; /* Indexing disabled. */
    }

    hierarchy->dyn_index.index_map = SelvaObject_New();
    hierarchy->dyn_index.ida = ida_init(FIND_INDICES_MAX_HINTS);

    /*
     * We allow a max of 2 * FIND_INDICES_MAX so we always have more to choose
     * from than we'll be able to create, and knowing that poptop will
     * periodically drop about half of the list, we want to avoid oscillating
     * too much.
     */
    poptop_init(&hierarchy->dyn_index.top_indices, 2 * selva_glob_config.find_indices_max, 0.0f);

    create_indexing_timer(ctx, hierarchy);
}

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

int SelvaFindIndex_Auto(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, RedisModuleString *dir_expression,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        RedisModuleString *order_field,
        RedisModuleString *filter,
        struct SelvaFindIndexControlBlock **icb_out) {
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
    if (!(dir & allowed_dirs)) {
        return SELVA_ENOTSUP;
    }

    struct icb_descriptor icb_desc = {
        .dir = dir,
        .dir_expression = dir_expression,
        .filter = filter,
        .sort = {
            .order = order,
            .order_field = order_field,
        },
    };

    icb = SelvaFindIndexICB_Pick(hierarchy, node_id, &icb_desc, upsert_icb(ctx, hierarchy, node_id, &icb_desc));
    *icb_out = icb;

    if (!icb || !icb->flags.valid) {
        return SELVA_ENOENT;
    }

    return 0;
}

int SelvaFindIndex_AutoMulti(
        RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, RedisModuleString *dir_expression,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        RedisModuleString *order_field,
        RedisModuleString *index_hints[],
        size_t nr_index_hints,
        struct SelvaFindIndexControlBlock *ind_icb_out[]) {
    int ind_select = -1;

    for (size_t i = 0; i < nr_index_hints; i++) {
        struct SelvaFindIndexControlBlock *icb = NULL;
        int err;

        /*
         * Hint: It's possible to disable ordered indices completely
         * by changing order here to SELVA_RESULT_ORDER_NONE.
         */
        err = SelvaFindIndex_Auto(ctx, hierarchy, dir, dir_expression, node_id, order, order_field, index_hints[i], &icb);
        ind_icb_out[i] = icb;
        if (!err) {
            if (icb &&
                (ind_select < 0 ||
                 SelvaFindIndex_IcbCard(icb) < SelvaFindIndex_IcbCard(ind_icb_out[ind_select]))) {
                ind_select = i; /* Select the smallest index res set for fastest lookup. */
            }
        } else if (err != SELVA_ENOENT && err != SELVA_ENOTSUP) {
            SELVA_LOG(SELVA_LOGL_ERR, "AutoIndex returned an error: %s\n",
                      getSelvaErrorStr(err));
        }
    }

    return ind_select;
}

int SelvaFindIndex_IsOrdered(
        struct SelvaFindIndexControlBlock *icb,
        enum SelvaResultOrder order,
        RedisModuleString *order_field) {
    return order != SELVA_RESULT_ORDER_NONE &&
           icb->sort.order == order &&
           icb->flags.ordered &&
           !RedisModule_StringCompare(icb->sort.order_field, order_field);
}

int SelvaFindIndex_Traverse(
        struct RedisModuleCtx *ctx,
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb,
        SelvaHierarchyNodeCallback node_cb,
        void * node_arg) {
    if (icb->flags.ordered) {
        struct SVectorIterator it;
        const struct TraversalOrderItem *item;

        SVector_ForeachBegin(&it, &icb->res.ord);
        while ((item = SVector_Foreach(&it))) {
            struct SelvaHierarchyNode *node;

            node = SelvaHierarchy_FindNode(hierarchy, item->node_id);
            if (node) {
                /*
                 * We should be breaking here if requested. This should only
                 * happen in case the index order is the same as requested
                 * order. Otherwise find shouldn't return 1 but use OrderItem
                 * subr.
                 */
                if (node_cb(ctx, hierarchy, node, node_arg)) {
                    break;
                }
            }
        }
    } else {
        struct SelvaSetElement *el;

        SELVA_SET_NODEID_FOREACH(el, &icb->res.set) {
            struct SelvaHierarchyNode *node;

            node = SelvaHierarchy_FindNode(hierarchy, el->value_nodeId);
            if (node) {
                /*
                 * Note that we don't break here on limit because limit and
                 * unordered indexing are incompatible. The reason is that we
                 * can't guarantee that the returned nodes would be the exactly
                 * same with and without indexing. However, find might still
                 * sort this response using the OrderItem subrs.
                 */
                (void)node_cb(ctx, hierarchy, node, node_arg);
            }
        }
    }

    return 0;
}

void SelvaFindIndex_Acc(struct SelvaFindIndexControlBlock * restrict icb, size_t acc_take, size_t acc_tot) {
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

void SelvaFindIndex_AccMulti(
        struct SelvaFindIndexControlBlock *ind_icb[],
        size_t nr_index_hints,
        int ind_select,
        size_t acc_take,
        size_t acc_tot) {
    for (int i = 0; i < (int)nr_index_hints; i++) {
        struct SelvaFindIndexControlBlock *icb = ind_icb[i];

        if (!icb) {
            continue;
        }

        if (i == ind_select) {
            SelvaFindIndex_Acc(icb, acc_take, acc_tot);
        } else if (ind_select == -1) {
            /* No index was selected so all will get the same take. */
            SelvaFindIndex_Acc(icb, acc_take, acc_tot);
        } else {
            /* Nothing taken from this index. */
            SelvaFindIndex_Acc(icb, 0, acc_tot);
        }
    }
}

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
                RedisModule_ReplyWithDouble(ctx, (double)SelvaFindIndex_IcbCard(icb));
            }
        } else if (type == SELVA_OBJECT_OBJECT) {
            n += list_index(ctx, (struct SelvaObject *)p);
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Unsupported index type: %s\n",
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
        /* Do not send Redis replies here. */
        return REDISMODULE_OK;
    }

    if (!is_indexing_active(hierarchy)) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
    }

    RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
    RedisModule_ReplySetArrayLength(ctx, 2 * list_index(ctx, hierarchy->dyn_index.index_map));

    return REDISMODULE_OK;
}

static int SelvaFindIndex_NewCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    int err;

    const int ARGV_REDIS_KEY = 1;
    const int ARGV_DIRECTION = 2;
    const int ARGV_REF_FIELD = 3;
    const int ARGV_ORDER_ORD = 4;
    const int ARGV_ORDER_FLD = 5;
    const int ARGV_NODE_ID = 6;
    const int ARGV_FILTER = 7;

    if (argc != 8) {
        return RedisModule_WrongArity(ctx);
    }

    /*
     * Open the Redis key.
     */
    SelvaHierarchy *hierarchy;
    hierarchy = SelvaModify_OpenHierarchy(ctx, argv[ARGV_REDIS_KEY], REDISMODULE_READ);
    if (!hierarchy) {
        /* Do not send Redis replies here. */
        return REDISMODULE_OK;
    }

    if (!is_indexing_active(hierarchy)) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
    }

    enum SelvaTraversal dir;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "Traversal direction");
    }

    RedisModuleString *dir_expression;
    if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        dir_expression = argv[ARGV_REF_FIELD];
    } else if ((dir & allowed_dirs) != 0) {
        dir_expression = NULL;
    } else {
        return replyWithSelvaErrorf(ctx, SELVA_ENOTSUP, "Traversal direction");
    }

    enum SelvaResultOrder order;
    err = SelvaTraversal_ParseOrder(&order, argv[ARGV_ORDER_ORD]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "order");
    }

    Selva_NodeId node_id;
    err = Selva_RMString2NodeId(node_id, argv[ARGV_NODE_ID]);
    if (err) {
        return replyWithSelvaErrorf(ctx, err, "node_id");
    }

    /*
     * Validate the expression.
     * This is not strictly necessary but it will save us from at least the most
     * obvious surprises with invalid expressions.
     */
    RedisModuleString *filter = argv[ARGV_FILTER];
    TO_STR(filter);
    struct rpn_expression *expr = rpn_compile(filter_str);
    rpn_destroy_expression(expr);
    if (!expr) {
        return replyWithSelvaError(ctx, SELVA_RPN_ECOMP);
    }

    /*
     * Create the index.
     */
    struct SelvaFindIndexControlBlock *icb = NULL;
    err = SelvaFindIndex_Auto(ctx, hierarchy,
            dir, dir_expression, node_id,
            order, order != SELVA_RESULT_ORDER_NONE ? argv[ARGV_ORDER_FLD] : NULL,
            filter, &icb);
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
    icb->pop_count.cur = 1000;
    icb->pop_count.ave = 1000.0f;
    icb->flags.permanent = 1;

    return RedisModule_ReplyWithLongLong(ctx, 1);
}

static int SelvaFindIndex_DelCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    SelvaHierarchy *hierarchy;
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
        /* Do not send Redis replies here. */
        return REDISMODULE_OK;
    }

    if (!is_indexing_active(hierarchy)) {
        return replyWithSelvaErrorf(ctx, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
    }

    size_t name_len;
    const char *name_str = RedisModule_StringPtrLen(argv[ARGV_INDEX], &name_len);
    err = SelvaFindIndexICB_Get(hierarchy, name_str, name_len, &icb);
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
