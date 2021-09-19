#pragma once
#ifndef SELVA_TUNABLES
#define SELVA_TUNABLES

/*
 * Generic tunables.
 */

/**
 * Debug memory usage.
 * 0 or undefined = Nothing
 * 1 = Clear some memory areas before freeing
 */
#define MEM_DEBUG 1

/*
 * SVector tunables.
 */

/**
 * Threshold to migrate from an SVECTOR_MODE_ARRAY to SVECTOR_MODE_RBTREE.
 */
#define SVECTOR_THRESHOLD 100

/**
 * How much memory to allocate when more memory is needed in
 * SVECTOR_MODE_RBTREE mode.
 */
#define SVECTOR_SLAB_SIZE 4194304

/*
 * Hierarchy tunables.
 */

/**
 * Initial vector lengths for children and parents lists.
 */
#define HIERARCHY_INITIAL_VECTOR_LEN    50

/**
 * Expected average length of a find response.
 */
#define HIERARCHY_EXPECTED_RESP_LEN     5000

/**
 * Sort hierarchy find results by depth.
 */
#define HIERARCHY_SORT_BY_DEPTH         0

/*
 * RPN Tunables.
 */

/**
 * Operand buffer size.
 * Small operands don't require malloc and are faster to operate with.
 * In general this should be at least 1 byte bigger than the nodeId size to keep
 * expressions operating on nodeIds fast. It should be also larger than the
 * size of a void pointer to make sure that if a pointer is stored in it but
 * the value is used as a string, still nothing extremely bad will happen.
 */
#define RPN_SMALL_OPERAND_SIZE          11

/**
 * Small operand pool size.
 * Small operands are pooled to speed up evaluating simple expressions.
 */
#define RPN_SMALL_OPERAND_POOL_SIZE     40

/**
 * Max RPN stack depth.
 */
#define RPN_MAX_D                       256

/*
 * Dynamic Find Query Index Tunables.
 */

#define FIND_INDICES_MAX_HINTS_FIND             20 /*!< Maximum number of indexing hints per find command. */
#define FIND_INDICES_MAX_HINTS                 500 /*!< Maximum number of indexing hints tracked. */
#define FIND_INDICES_MAX                       100 /*!< Maximum number of indices. 0 = disable indexing. */
#define FIND_INDEXING_THRESHOLD                100 /*!< A candidate for indexing must have at least this many visits per traversal. */
#define FIND_INDEXING_ICB_UPDATE_INTERVAL     1000 /*!< ICB refresh interval [ms] */
#define FIND_INDEXING_INTERVAL               60000 /*! How often the set of active indices is decided. */
#define FIND_INDEXING_POPULARITY_AVE_PERIOD 216000 /*!< [sec] Averaging period for indexing hint demand count. After this period the original value is reduced to 1/e * n. */

/*
 * Async_task Tunables.
 */

/**
 * Drop all messages.
 */
#define ASYNC_TASK_DEBUG_DROP_ALL       0

/**
 * Task buffer block size.
 * Tasks are sent over fixed size buffers but a single task can occupy multiple
 * buffers. This value should be big enough to fit majority of tasks sent in
 * normal operation but also small enough so no space is wasted for padding,
 * as the buffers can't be split.
 * It might be a good idea to set this to a multiple of the cache line size.
 */
#define ASYNC_TASK_RING_BUF_BLOCK_SIZE  64

/**
 * Number of task buffer blocks per worker.
 */
#define ASYNC_TASK_RING_BUF_LENGTH      1000000

/**
 * Async task peek interval.
 */
#define ASYNC_TASK_PEEK_INTERVAL_NS     500000L

/**
 * Number of async task workers.
 */
#define ASYNC_TASK_HIREDIS_WORKER_COUNT 4

#endif /* SELVA_TUNABLES */
