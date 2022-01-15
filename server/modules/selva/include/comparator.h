#pragma once
#ifndef COMPARATOR
#define COMPARATOR

int SelvaSVectorComparator_Cstring(const void ** restrict ap, const void ** restrict bp);
int SelvaSVectorComparator_NodeId(const void ** restrict ap, const void ** restrict bp);
int SelvaSVectorComparator_RMS(const void ** restrict ap, const void ** restrict bp);

/**
 * Compare two Selva Hierarchy nodes.
 */
int SelvaSVectorComparator_Node(const void ** restrict a, const void ** restrict b);

#endif /* COMPARATOR */
