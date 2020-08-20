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

#define RPN_MAX_TOKEN_SIZE              15
#define RPN_SMALL_OPERAND_SIZE          11
#define RPN_SMALL_OPERAND_POOL_SIZE     20
#define RPN_MAX_D                       256

/*
 * Async_task Tunables.
 */

#define ASYNC_TASK_RING_BUF_BLOCK_SIZE  128
#define ASYNC_TASK_RING_BUF_LENGTH      500000
#define ASYNC_TASK_PEEK_INTERVAL_US     100
#define ASYNC_TASK_HIREDIS_WORKER_COUNT 4

#endif /* SELVA_MODIFY_TUNABLES */
