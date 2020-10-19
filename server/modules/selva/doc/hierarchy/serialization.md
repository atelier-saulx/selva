Hierarchy Serialization
=======================

Hierarchy trees are serialized starting from each head towards descendants using
DFS algorithm.

The format is as follows: A nodeId is written first, then the number of children
that node has, and finally list the ID of each child. Then the algorithm
proceeds to the next node according to DFS algorithm. Finally once all nodes
of the whole hierarchy have been visited, a special id `HIERARCHY_RDB_EOF` (0)
is written marking the end of the serialized hierarchy.

The final serialization result looks like this:

```
NODE_ID1 | NR_CHILDREN | CHILD_ID_0,..
NODE_ID2 | NR_CHILDREN | ...
HIERARCHY_RDB_EOF
```

Deserialization is practically a reverse operation of what the serialization
algorithm does. A nodeId is read first but it's kept in memory for now. Then the
number of children is read, which tells how many values to read as child IDs.
The children are created first, one by one. Finally once all the child nodes
have been created the parent node itself can be created and marked as a parent
to the new children. The algorithm is repeated until `HIERARCHY_RDB_EOF` is
reached.
