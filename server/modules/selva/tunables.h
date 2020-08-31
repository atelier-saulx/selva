#pragma once
#ifndef SELVA_MODIFY_TUNABLES
#define SELVA_MODIFY_TUNABLES

/*
 * Hierarchy tunables.
 */

/**
 * Initial vector lengths for children and parents lists.
 */
#define HIERARCHY_INITIAL_VECTOR_LEN    100

/**
 * Expected average length of a find response.
 */
#define HIERARCHY_EXPECTED_RESP_LEN     5000

/**
 * Sort hierarchy find results by depth.
 */
#define HIERARCHY_SORT_BY_DEPTH         0

/**
 * Send events to descendants when a new parent is added.
 */
#define HIERARCHY_EN_ANCESTORS_EVENTS   1

/*
 * RPN Tunables.
 */

/**
 * Maximum size of a single token in an RPN expression.
 * An operand in an expression cannot exceed this length.
 */
#define RPN_MAX_TOKEN_SIZE              15

/**
 * Operand buffer size.
 * Small operands don't require malloc and are faster to
 * operate with. In general this should be at least 1 byte
 * bigger than the nodeId size to keep expressions operating
 * on nodeIds fast.
 */
#define RPN_SMALL_OPERAND_SIZE          11

/**
 * Small operand pool size.
 * Small operands are pooled to speed up evaluating simple expressions.
 */
#define RPN_SMALL_OPERAND_POOL_SIZE     20

/**
 * Max RPN stack depth.
 */
#define RPN_MAX_D                       256

/*
 * Async_task Tunables.
 */

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
#define ASYNC_TASK_PEEK_INTERVAL_US     100

/**
 * Number of async task workers.
 */
#define ASYNC_TASK_HIREDIS_WORKER_COUNT 4

#endif /* SELVA_MODIFY_TUNABLES */
