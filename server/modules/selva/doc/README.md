Selva Redis Module
==================

Hierarchy
---------

Hierarchy is an acyclic graph data structure used for storing the
hierarchical relationships between the nodes in Selva.

Hierarchy implements a number of different ways to traverse the graph.
The traversal methods can also filter out nodes from the results by
using [RPN Filter Expressions](hierarchy/expressions.md) which can
most importantly access the node fields and do comparison and boolean
operations against the fields.

The serialization format of `hierarchy` is documented separately in
[Serialization](hierarchy/serialization.md).

Selva Object
------------

`SelvaObject` is a new Redis data type for storing object-like data in
compute-efficient manner. It can currently store strings, integers, and
doubles natively in the most efficient format for access and computation.
Depending on the use case, the measured performance improvement over
Redis hash type is 20 % - 25 %. It can potentially offer even greater
performance improvement, compared to hashes, in use cases where the data
structure is accessed directly inside a Redis module.

The data type is implemented in [selva_object](../module/selva_object.c).

Selva Node
----------

A Selva node is built from several parts stored in different ways in Redis and
therefore also the management and ownership of the data is divided into
separate C files.

Regular node fields are generally managed by
[selva_node.c](../module/selva_node.c), which is used to abstract/wrap all
access to regular node fields, including creation, reading, and writing of
all regular fields. It's also called to create the initial node when a new
node with no user provided fields is created.

Set fields of a node are managed by [selva_set.c](../module/selva_set.c). This
includes both, regular string sets as well as node reference sets. However,
the reference fields are be also resolved and traversed by
[hierarchy.c](../module/hierarchy.c).

Node aliases (and `aliases` field) are handled by [alias.c](../module/alias.c).

Hierarchy fields `ancestors`, `children`, `descendants`, and `parents` are
owned, managed, and traversed by the functions in
[hierarchy.c](../module/hierarchy.c).

Commands
--------

### Find Commands

See [find.c](../module/find.c).

### Hierarchy Commands

See [hierarchy.c](../module/hierarchy.c).

### Subscription Commands

See [subscriptions.c](../module/subscriptions.c).

### Modify

Create or update Selva nodes.

See [modify.c](../module/modify.c).

### Selva Objects

See [selva_object](../module/selva_object.c).
