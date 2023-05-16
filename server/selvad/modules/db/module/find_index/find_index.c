/*
 * Copyright (c) 2021-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <tgmath.h>
#include <time.h>
#include "jemalloc.h"
#include "util/bitmap.h"
#include "util/ctime.h"
#include "util/finalizer.h"
#include "util/ida.h"
#include "util/lpf.h"
#include "util/poptop.h"
#include "util/selva_string.h"
#include "event_loop.h"
#include "selva_error.h"
#include "selva_log.h"
#include "selva_proto.h"
#include "selva_server.h"
#include "db_config.h"
#include "hierarchy.h"
#include "selva_db.h"
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

static void create_icb_timer(struct SelvaFindIndexControlBlock *icb);
static void create_indexing_timer(struct SelvaHierarchy *hierarchy);

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
    return SelvaTraversal_GetSkip(icb->traversal.dir) && !memcmp(node_id, icb->node_id, SELVA_NODE_ID_SIZE);
}

static void icb_res_init(struct SelvaFindIndexControlBlock *icb) {
    if (icb->flags.ordered) {
        const size_t initial_len = (size_t)icb->find_acc.take_max_ave;

        SelvaTraversalOrder_InitOrderResult(&icb->res.ord, icb->traversal.sort.order, initial_len);
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
        struct selva_string *lang = NULL;

        item = SelvaTraversalOrder_CreateNodeOrderItem(NULL, lang, node, icb->traversal.sort.order_field);
        if (SVector_InsertFast(&icb->res.ord, item)) {
            SelvaTraversalOrder_DestroyOrderItem(item);
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
         * Delete the res to trigger a full rebuild.
         */
        if (icb->flags.valid) {
            Selva_NodeId node_id;

            SelvaHierarchy_GetNodeId(node_id, node);
            SELVA_LOG(SELVA_LOGL_DBG, "The index must be purged and refreshed because %.*s was removed",
                      (int)SELVA_NODE_ID_SIZE, node_id);

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

        if (Selva_SubscriptionFilterMatch(hierarchy, node, marker)) {
            if (!skip_node(icb, node)) {
                Selva_NodeId node_id;

                SelvaHierarchy_GetNodeId(node_id, node);
                SELVA_LOG(SELVA_LOGL_DBG, "Adding node %.*s to the index after refresh",
                          (int)SELVA_NODE_ID_SIZE, node_id);

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
            const char *order_field_str = selva_string_to_str(icb->traversal.sort.order_field, &order_field_len);

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
            } else if (Selva_SubscriptionFilterMatch(hierarchy, node, marker)) {
                Selva_NodeId node_id;

                icb_res_add(icb, node);

                SelvaHierarchy_GetNodeId(node_id, node);
                SELVA_LOG(SELVA_LOGL_DBG, "Adding node %.*s to the index",
                          (int)SELVA_NODE_ID_SIZE, node_id);
            }
        }
    } else {
        Selva_NodeId node_id;

        SelvaHierarchy_GetNodeId(node_id, node);
        SELVA_LOG(SELVA_LOGL_DBG, "NOP %x for node %.*s",
                  (unsigned)event_flags, (int)SELVA_NODE_ID_SIZE, node_id);
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
        dir_expression = selva_string_to_str(icb->traversal.dir_expression, NULL);
    }

    err = SelvaSubscriptions_AddCallbackMarker(
            hierarchy, find_index_sub_id, icb->marker_id, marker_flags,
            icb->node_id, icb->traversal.dir, dir_field, dir_expression, selva_string_to_str(icb->traversal.filter, NULL),
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
 * @param hierarchy is a pointer to the hierarchy.
 * @param icb is a pointer to the indexed ICB.
 */
static int refresh_index(
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
    err = SelvaSubscriptions_RefreshByMarkerId(hierarchy, find_index_sub_id, icb->marker_id);
    SELVA_TRACE_END(FindIndex_refresh);

    return err;
}

/**
 * Discard an index.
 * Discard the index and make it inactive to avoid rebuilding.
 */
static int discard_index(
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    int err;

    poptop_remove(&hierarchy->dyn_index.top_indices, icb);

    if (hierarchy) {
        if (icb->flags.valid_marked_id) {
            err = SelvaSubscriptions_DeleteMarker(hierarchy, find_index_sub_id, icb->marker_id);
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
 * Destroy an index control block and free all the memory.
 */
__attribute__((nonnull (1, 2))) static int destroy_icb(
        struct SelvaHierarchy *hierarchy,
        struct SelvaFindIndexControlBlock *icb) {
    int err;

    SELVA_LOG(SELVA_LOGL_DBG, "Destroying icb for %.*s",
              (int)icb->name_len, icb->name_str);

    err = discard_index(hierarchy, icb);
    if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to discard an index for \"%.*s\". err: \"%s\"",
                  (int)icb->name_len, icb->name_str,
                  selva_strerror(err));
        return err;
    }

    err = SelvaFindIndexICB_Del(hierarchy, icb);
    if (err && err != SELVA_ENOENT) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy an index for \"%.*s\". err: \"%s\"",
                  (int)icb->name_len, icb->name_str,
                  selva_strerror(err));
        return err;
    }

    if (icb->flags.valid_marked_id) {
        ida_free(hierarchy->dyn_index.ida, (ida_t)icb->marker_id);
    }

    if (icb->traversal.dir_expression) {
        selva_string_free(icb->traversal.dir_expression);
    }

    if (icb->traversal.filter) {
        selva_string_free(icb->traversal.filter);
    }

    if (icb->flags.valid_timer_id) {
        evl_clear_timeout(icb->timer_id, NULL);
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
static void make_indexing_decission_proc(struct event *e __unused, void *data) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_make_indexing_decission_proc);
    SelvaHierarchy *hierarchy = (struct SelvaHierarchy *)data;
    struct poptop_list_el *el;

#if 0
    SELVA_LOG(SELVA_LOGL_DBG, "TOCK");
#endif

    hierarchy->dyn_index.proc_timer_active = 0;
    create_indexing_timer(hierarchy);

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

                SELVA_LOG(SELVA_LOGL_DBG, "Discarding index for \"%.*s\"",
                          (int)icb->name_len, icb->name_str);

                err = discard_index(hierarchy, icb);
                if (err) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to discard the index for \"%.*s\". err: \"%s\"",
                              (int)icb->name_len, icb->name_str,
                              selva_strerror(err));
                }
            } else {
                /*
                 * Destroy the index.
                 * The hint hasn't been seen for a some time and we are going to
                 * discard the index and destroy the ICB.
                 */
                int err;

                SELVA_LOG(SELVA_LOGL_DBG, "Destroying index for \"%.*s\"",
                          (int)icb->name_len, icb->name_str);

                err = destroy_icb(hierarchy, icb);
                if (err) {
                    SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy the index for \"%.*s\". err: \"%s\"",
                              (int)icb->name_len, icb->name_str,
                              selva_strerror(err));
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
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to create an index for \"%.*s\". err: \"%s\"",
                          (int)icb->name_len, icb->name_str,
                          selva_strerror(err));
            } else {
                SELVA_LOG(SELVA_LOGL_DBG, "Created an index for \"%.*s\"",
                          (int)icb->name_len, icb->name_str);
            }
        }

        /*
         * Since we are using the SELVA_SUBSCRIPTION_FLAG_REFRESH flag the call to
         * refresh will call the action function for each node and thus build the
         * initial index.
         */
        err = refresh_index(hierarchy, icb);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to refresh the index for \"%.*s\". err: \"%s\"",
                      (int)icb->name_len, icb->name_str,
                      selva_strerror(err));

            /*
             * Destroy the ICB because it's likely that creating this index would
             * fail on every further attempt.
             * This should be relatively safe as the removal won't change the
             * ordering of top_indices.
             */
            err = destroy_icb(hierarchy, icb);
            if (err) {
                SELVA_LOG(SELVA_LOGL_ERR, "Failed to destroy the index for \"%.*s\". err: \"%s\"",
                          (int)icb->name_len, icb->name_str,
                          selva_strerror(err));
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
static void icb_proc(struct event *e __unused, void *data) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_icb_proc);
    struct SelvaFindIndexControlBlock *icb = (struct SelvaFindIndexControlBlock *)data;

#if 0
    SELVA_LOG(SELVA_LOGL_DBG, "TICK");
#endif

    /* Recreate the timer. */
    icb->flags.valid_timer_id = 0;
    create_icb_timer(icb);

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
        SELVA_LOG(SELVA_LOGL_DBG, "Maybe added %.*s:%p to poptop with score: %f",
                  (int)icb->name_len, icb->name_str,
                  icb,
                  score);
    }
}

/**
 * Get or create an indexing control block.
 */
static struct SelvaFindIndexControlBlock *upsert_icb(
        struct finalizer *fin,
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
            if (err == SELVA_ENOBUFS) {
                SELVA_LOG(SELVA_LOGL_DBG,
                          "FIND_INDICES_MAX_HINTS reached. Destroying \"%.*s\": %s\n",
                          (int)icb->name_len, icb->name_str,
                          selva_strerror(err));
            } else {
                SELVA_LOG(SELVA_LOGL_ERR,
                          "Failed to get a new marker id for an index \"%.*s\": %s\n",
                          (int)icb->name_len, icb->name_str,
                          selva_strerror(err));
            }

            destroy_icb(hierarchy, icb);
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
        memcpy(icb->node_id, node_id, SELVA_NODE_ID_SIZE);

        /* Note that dir_field is not supported for indexing. */
        icb->traversal = *desc;
        if (icb->traversal.dir_expression) {
            finalizer_del(fin, icb->traversal.dir_expression);
        }
        finalizer_del(fin, icb->traversal.filter);

        if (icb->traversal.sort.order != SELVA_RESULT_ORDER_NONE) {
            icb->flags.ordered = 1;
            finalizer_del(fin, icb->traversal.sort.order_field);
        }

        /*
         * Map the newly created icb into the dyn_index SelvaObject.
         */
        err = SelvaFindIndexICB_Set(hierarchy, name_str, name_len, icb);
        if (err) {
            SELVA_LOG(SELVA_LOGL_ERR, "Failed to insert a new ICB at \"%.*s\". err: \"%s\"",
                      (int)name_len, name_str,
                      selva_strerror(err));

            destroy_icb(hierarchy, icb);
            return NULL;
        }

        /* Finally create a proc timer. */
        create_icb_timer(icb);
    } else if (err) {
        SELVA_LOG(SELVA_LOGL_ERR, "Get ICB for \"%.*s\" failed. err: \"%s\"",
                  (int)name_len, name_str,
                  selva_strerror(err));

        icb = NULL;
    }

    return icb;
}

/**
 * Create a timer for an ICB.
 * Generally every ICB has a timer.
 */
static void create_icb_timer(struct SelvaFindIndexControlBlock *icb) {
    const struct timespec period = MSEC2TIMESPEC(selva_glob_config.find_indexing_icb_update_interval);

    assert(icb->flags.valid_timer_id == 0);
    icb->timer_id = evl_set_timeout(&period, icb_proc, icb);
    icb->flags.valid_timer_id = 1;

    if (icb->timer_id < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to setup an ICB timer");
    }
}

/**
 * Create a timer for indexing the hierarchy.
 * Only one timer per hierarchy should be created.
 */
static void create_indexing_timer(struct SelvaHierarchy *hierarchy) {
    const struct timespec period = MSEC2TIMESPEC(selva_glob_config.find_indexing_interval);

    assert(hierarchy->dyn_index.proc_timer_active == 0);
    hierarchy->dyn_index.proc_timer_id = evl_set_timeout(&period, make_indexing_decission_proc, hierarchy);
    hierarchy->dyn_index.proc_timer_active = 1;

    if (hierarchy->dyn_index.proc_timer_id < 0) {
        SELVA_LOG(SELVA_LOGL_ERR, "Failed to setup the indexing timer");
    }
}

void SelvaFindIndex_Init(SelvaHierarchy *hierarchy) {
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

    create_indexing_timer(hierarchy);
}

static void deinit_index_obj(struct SelvaHierarchy *hierarchy, struct SelvaObject *obj) {
    SelvaObject_Iterator *it;
    enum SelvaObjectType type;
    void *p;

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, NULL, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            (void)destroy_icb(hierarchy, (struct SelvaFindIndexControlBlock *)p);
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
        evl_clear_timeout(hierarchy->dyn_index.proc_timer_id, NULL);
    }

    memset(&hierarchy->dyn_index, 0, sizeof(hierarchy->dyn_index));
}

int SelvaFindIndex_Auto(
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, struct selva_string *dir_expression,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        struct selva_string *order_field,
        struct selva_string *filter,
        struct SelvaFindIndexControlBlock **icb_out) {
    SELVA_TRACE_BEGIN_AUTO(FindIndex_AutoIndex);
    __auto_finalizer struct finalizer fin;
    struct SelvaFindIndexControlBlock *icb;

    finalizer_init(&fin);

    if (selva_string_get_len(filter) == 0 || selva_glob_config.find_indices_max == 0) {
        /* TODO should it be: find_indices_max == 0 => SELVA_ENOTSUP? */
        return SELVA_EINVAL;
    }

    /*
     * Only index some traversals.
     */
    if (!(dir & allowed_dirs)) {
        return SELVA_ENOTSUP;
    }

    /*
     * Copy the strings.
     */
    if (dir_expression) {
        dir_expression = selva_string_dup(dir_expression, 0);
        selva_string_auto_finalize(&fin, dir_expression);
    }
    if (order_field) {
        order_field = selva_string_dup(order_field, 0);
        selva_string_auto_finalize(&fin, order_field);
    }
    filter = selva_string_dup(filter, 0);
    selva_string_auto_finalize(&fin, filter);

    struct icb_descriptor icb_desc = {
        .dir = dir,
        .dir_expression = dir_expression,
        .filter = filter,
        .sort = {
            .order = order,
            .order_field = order_field,
        },
    };

    icb = SelvaFindIndexICB_Pick(hierarchy, node_id, &icb_desc, upsert_icb(&fin, hierarchy, node_id, &icb_desc));
    *icb_out = icb;

    if (!icb || !icb->flags.valid) {
        return SELVA_ENOENT;
    }

    return 0;
}

int SelvaFindIndex_AutoMulti(
        struct SelvaHierarchy *hierarchy,
        enum SelvaTraversal dir, struct selva_string *dir_expression,
        const Selva_NodeId node_id,
        enum SelvaResultOrder order,
        struct selva_string *order_field,
        struct selva_string *index_hints[],
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
        err = SelvaFindIndex_Auto(hierarchy, dir, dir_expression, node_id, order, order_field, index_hints[i], &icb);
        ind_icb_out[i] = icb;
        if (!err) {
            if (icb &&
                (ind_select < 0 ||
                 SelvaFindIndex_IcbCard(icb) < SelvaFindIndex_IcbCard(ind_icb_out[ind_select]))) {
                ind_select = i; /* Select the smallest index res set for fastest lookup. */
            }
        } else if (err != SELVA_ENOENT && err != SELVA_ENOTSUP) {
            SELVA_LOG(SELVA_LOGL_ERR, "AutoIndex returned an error: \"%s\"",
                      selva_strerror(err));
        }
    }

    return ind_select;
}

int SelvaFindIndex_IsOrdered(
        struct SelvaFindIndexControlBlock *icb,
        enum SelvaResultOrder order,
        struct selva_string *order_field) {
    return order != SELVA_RESULT_ORDER_NONE &&
           icb->traversal.sort.order == order &&
           icb->flags.ordered &&
           !selva_string_cmp(icb->traversal.sort.order_field, order_field);
}

int SelvaFindIndex_Traverse(
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
                if (node_cb(hierarchy, node, node_arg)) {
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
                (void)node_cb(hierarchy, node, node_arg);
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

static int list_index(struct selva_server_response_out *resp, struct SelvaObject *obj) {
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
            selva_send_str(resp, icb->name_str, icb->name_len);
            selva_send_array(resp, 4);
            selva_send_double(resp, (double)icb->find_acc.take_max_ave);
            selva_send_double(resp, (double)icb->find_acc.tot_max_ave);
            selva_send_double(resp, (double)icb->find_acc.ind_take_max_ave);
            if (!icb->flags.active) {
                selva_send_str(resp, "not_active", 10);
            } else if (!icb->flags.valid) {
                selva_send_str(resp, "not_valid", 9);
            } else {
                selva_send_double(resp, (double)SelvaFindIndex_IcbCard(icb));
            }
        } else if (type == SELVA_OBJECT_OBJECT) {
            n += list_index(resp, (struct SelvaObject *)p);
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Unsupported index type: %s",
                      SelvaObject_Type2String(type, NULL));
        }
    }

    return n;
}

static int _debug_index(struct selva_server_response_out *resp, struct SelvaHierarchy *hierarchy, struct SelvaObject *obj, int n, int i) {
    SelvaObject_Iterator *it;
    enum SelvaObjectType type;
    void *p;

    if (!obj) {
        return n;
    }

    it = SelvaObject_ForeachBegin(obj);
    while ((p = SelvaObject_ForeachValueType(obj, &it, NULL, &type))) {
        if (type == SELVA_OBJECT_POINTER) {
            const struct SelvaFindIndexControlBlock *icb = (const struct SelvaFindIndexControlBlock *)p;

            if (n++ == i) {
                struct Selva_SubscriptionMarker *marker;

                marker = SelvaSubscriptions_GetMarker(hierarchy, find_index_sub_id, icb->marker_id);
                if (marker) {
                    SelvaSubscriptions_ReplyWithMarker(resp, marker);
                } else {
                    selva_send_null(resp);
                }

                return n;
            }
        } else if (type == SELVA_OBJECT_OBJECT) {
            n += _debug_index(resp, hierarchy, (struct SelvaObject *)p, n, i);
            if (n > i) {
                return n;
            }
        } else {
            SELVA_LOG(SELVA_LOGL_ERR, "Unsupported index type: %s",
                      SelvaObject_Type2String(type, NULL));
        }
    }

    return n;
}
static void debug_index(struct selva_server_response_out *resp, struct SelvaHierarchy *hierarchy, int i) {
    struct SelvaObject *obj = hierarchy->dyn_index.index_map;

    if (_debug_index(resp, hierarchy, obj, 0, i) <= i) {
        selva_send_null(resp);
    }
}

static void SelvaFindIndex_ListCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    SelvaHierarchy *hierarchy;

    if (len != 0) {
        selva_send_error_arity(resp);
        return;
    }

    hierarchy = main_hierarchy;

    if (!is_indexing_active(hierarchy)) {
        selva_send_errorf(resp, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
        return;
    }

    selva_send_array(resp, -1);
    (void)list_index(resp, hierarchy->dyn_index.index_map);
    /* Sent 2 * res */
}

static void SelvaFindIndex_NewCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    SelvaHierarchy *hierarchy = main_hierarchy;
    __auto_finalizer struct finalizer fin;
    struct selva_string **argv = NULL;
    int argc;
    int err;

    finalizer_init(&fin);

    const int ARGV_DIRECTION = 0;
    const int ARGV_REF_FIELD = 1;
    const int ARGV_ORDER_ORD = 2;
    const int ARGV_ORDER_FLD = 3;
    const int ARGV_NODE_ID = 4;
    const int ARGV_FILTER = 5;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc != 6) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    if (!is_indexing_active(hierarchy)) {
        selva_send_errorf(resp, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
        return;
    }

    enum SelvaTraversal dir;
    err = SelvaTraversal_ParseDir2(&dir, argv[ARGV_DIRECTION]);
    if (err) {
        selva_send_errorf(resp, err, "Traversal direction");
        return;
    }

    struct selva_string *dir_expression;
    if (dir == SELVA_HIERARCHY_TRAVERSAL_BFS_EXPRESSION) {
        dir_expression = argv[ARGV_REF_FIELD];
    } else if ((dir & allowed_dirs) != 0) {
        dir_expression = NULL;
    } else {
        selva_send_errorf(resp, SELVA_ENOTSUP, "Traversal direction");
        return;
    }

    enum SelvaResultOrder order;
    err = SelvaTraversal_ParseOrder(&order, argv[ARGV_ORDER_ORD]);
    if (err) {
        selva_send_errorf(resp, err, "order");
        return;
    }

    Selva_NodeId node_id;
    err = selva_string2node_id(node_id, argv[ARGV_NODE_ID]);
    if (err) {
        selva_send_errorf(resp, err, "node_id");
        return;
    }

    /*
     * Validate the expression.
     * This is not strictly necessary but it will save us from at least the most
     * obvious surprises with invalid expressions.
     */
    struct selva_string *filter = argv[ARGV_FILTER];
    TO_STR(filter);
    struct rpn_expression *expr = rpn_compile(filter_str);
    rpn_destroy_expression(expr);
    if (!expr) {
        selva_send_error(resp, SELVA_RPN_ECOMP, NULL, 0);
        return;
    }

    /*
     * Create the index.
     */
    struct SelvaFindIndexControlBlock *icb = NULL;
    err = SelvaFindIndex_Auto(
            hierarchy,
            dir, dir_expression, node_id,
            order, order != SELVA_RESULT_ORDER_NONE ? argv[ARGV_ORDER_FLD] : NULL,
            filter, &icb);
    if ((err && err != SELVA_ENOENT) || !icb) {
        selva_send_errorf(resp, err, "Failed to create an index");
        return;
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

    selva_send_ll(resp, 1);
}

static void SelvaFindIndex_DelCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;
    struct SelvaFindIndexControlBlock *icb;
    int discard = 0;
    int err;

    finalizer_init(&fin);

    const int ARGV_INDEX = 0;
    const int ARGV_OP = 1;

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc < 0) {
        selva_send_errorf(resp, argc, "Failed to parse args");
        return;
    }

    if (argc == 2) {
        const struct selva_string *op = argv[ARGV_OP];
        TO_STR(op);

        if (op_len == 1 && op_str[0] == '0') {
            discard = 0;
        } else if (op_len == 1 && op_str[0] == '1') {
            discard = 1;
        } else {
            selva_send_error(resp, SELVA_ENOTSUP, NULL, 0);
            return;
        }
    } else if (argc != 3) {
        selva_send_error_arity(resp);
        return;
    }

    if (!is_indexing_active(hierarchy)) {
        selva_send_errorf(resp, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
        return;
    }

    size_t name_len;
    const char *name_str = selva_string_to_str(argv[ARGV_INDEX], &name_len);
    err = SelvaFindIndexICB_Get(hierarchy, name_str, name_len, &icb);
    if (err == SELVA_ENOENT) {
        selva_send_ll(resp, 0);
        return;
    } else if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }

    if (discard) {
        err = discard_index(hierarchy, icb);
    } else {
        err = destroy_icb(hierarchy, icb);
    }
    if (err) {
        selva_send_error(resp, err, NULL, 0);
        return;
    }

    selva_send_ll(resp, 1);
}

static void SelvaFindIndex_DebugCommand(struct selva_server_response_out *resp, const void *buf, size_t len) {
    __auto_finalizer struct finalizer fin;
    SelvaHierarchy *hierarchy = main_hierarchy;
    struct selva_string **argv;
    int argc;

    finalizer_init(&fin);

    const int ARGV_INDEX = 0; /* Numeric index. */

    argc = selva_proto_buf2strings(&fin, buf, len, &argv);
    if (argc != 1) {
        if (argc < 0) {
            selva_send_errorf(resp, argc, "Failed to parse args");
        } else {
            selva_send_error_arity(resp);
        }
        return;
    }

    if (!is_indexing_active(hierarchy)) {
        selva_send_errorf(resp, SELVA_ENOENT, INDEX_ERR_MSG_DISABLED);
        return;
    }

    long long i = -1;

    selva_string_to_ll(argv[ARGV_INDEX], &i);
    debug_index(resp, hierarchy, (int)((i - 1) / 2));
}

static void SelvaFindIndex_InfoCommand(struct selva_server_response_out *resp, const void *buf __unused, size_t len) {
    if (len) {
        selva_send_error_arity(resp);
        return;
    }

    selva_send_array(resp, 2);
    selva_send_str(resp, "lpf_a", 5);
    selva_send_ll(resp, lpf_a);
}

static int FindIndex_OnLoad(void) {
    lpf_a = lpf_geta((float)selva_glob_config.find_indexing_popularity_ave_period, (float)selva_glob_config.find_indexing_icb_update_interval / 1000.0f);

    selva_mk_command(CMD_ID_INDEX_LIST, SELVA_CMD_MODE_PURE, "index.list", SelvaFindIndex_ListCommand);
    selva_mk_command(CMD_ID_INDEX_NEW, SELVA_CMD_MODE_PURE, "index.new", SelvaFindIndex_NewCommand);
    selva_mk_command(CMD_ID_INDEX_DEL, SELVA_CMD_MODE_PURE, "index.del", SelvaFindIndex_DelCommand);
    selva_mk_command(CMD_ID_INDEX_DEBUG, SELVA_CMD_MODE_PURE, "index.debug", SelvaFindIndex_DebugCommand);
    selva_mk_command(CMD_ID_INDEX_INFO, SELVA_CMD_MODE_PURE, "index.info", SelvaFindIndex_InfoCommand);

    return 0;
}
SELVA_ONLOAD(FindIndex_OnLoad);
