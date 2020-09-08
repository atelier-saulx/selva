#pragma once
#ifndef _SELVA_
#define _SELVA_

#define SELVA_NODE_ID_SIZE      10ul
#define SELVA_NODE_TYPE_SIZE    2
#define ROOT_NODE_ID            "root\0\0\0\0\0\0"

/**
 * Type for Selva NodeId.
 */
typedef char Selva_NodeId[SELVA_NODE_ID_SIZE];

#endif /* _SELVA_ */
