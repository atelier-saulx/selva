/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/*
 * Generic tunables.
 */

/**
 * Debug memory usage.
 * 0 or undefined = Nothing
 * 1 = Clear some memory areas before freeing
 */
#define MEM_DEBUG 1

/**
 * Add delay to the replication of the Modify command.
 * Unit is nanoseconds. Normally this should be set to 0.
 */
#define DEBUG_MODIFY_REPLICATION_DELAY_NS 0

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
 * Server tunables.
 */

/**
 * Maximum number of client connections.
 */
#define SERVER_MAX_CLIENTS 100

/**
 * Server TCP backlog size.
 */
#define SERVER_BACKLOG_SIZE 10

/*
 * Hierarchy tunables.
 */

/**
 * Hierarchy node pool slab size in bytes.
 */
#define HIERARCHY_SLAB_SIZE 33554432

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

/**
 * Compression level used for compressing subtrees.
 * Range: 1 - 12
 */
#define HIERARCHY_COMPRESSION_LEVEL 6

/**
 * Attempt to compress inactive nodes in-memory.
 * 0 Disables automatic compression.
 */
#define HIERARCHY_AUTO_COMPRESS_PERIOD_MS 0

/**
 * Hierarchy auto compression transaction age limit.
 */
#define HIERARCHY_AUTO_COMPRESS_OLD_AGE_LIM 100

/**
 * How many inactive nodes can be tracked simultaneously.
 * Adjusting this shouldn't affect much unless the lest remains full all the
 * time.
 */
#define HIERARCHY_AUTO_COMPRESS_INACT_NODES_LEN (4096 / SELVA_NODE_ID_SIZE)

/*
 * Command tunables.
 */

/**
 * Maximum number of update operations on a sinlge command.
 */
#define SELVA_CMD_UPDATE_MAX 300

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
#define RPN_SMALL_OPERAND_POOL_SIZE     70

/**
 * Max RPN stack depth.
 */
#define RPN_MAX_D                       256

/**
 * Max number of forward jump labels in a single expression.
 */
#define RPN_MAX_LABELS                  128

/*
 * Dynamic Find Query Index Tunables.
 */

#define FIND_INDICES_MAX_HINTS_FIND             20 /*!< Maximum number of indexing hints per find command. */
#define FIND_INDICES_MAX_HINTS                 500 /*!< Maximum number of indexing hints tracked. */
#define FIND_INDICES_MAX                         0 /*!< Maximum number of indices. 0 = disable indexing. */
#define FIND_INDEXING_THRESHOLD                100 /*!< A candidate for indexing must have at least this many visits per traversal. */
#define FIND_INDEXING_ICB_UPDATE_INTERVAL     5000 /*!< [ms] ICB refresh interval. */
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
