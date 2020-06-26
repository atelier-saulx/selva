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

/*
 * RPN Tunables.
 */

#define RPN_SMALL_OPERAND_SIZE          11
#define RPN_SMALL_OPERAND_POOL_SIZE     20
#define RPN_MAX_D                       256

#endif /* SELVA_MODIFY_TUNABLES */
