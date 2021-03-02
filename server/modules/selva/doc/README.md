# Selva Redis Module

## Hierarchy

Hierarchy is an acyclic graph data structure used for storing the
hierarchical relationships between the nodes in Selva. Hierarchy
implements parent and child constraints that have a very specific
behavior when executing certain operations on the hierarchy.

Hierarchy implements a number of different ways to traverse the graph
utilizing no only the family tree -like relationships but also other
possible reference and relationship types. The traversal methods can
also filter out nodes from the results by using
[RPN Filter Expressions](hierarchy/expressions.md) with the traversal
methods, which can, most importantly, access the node fields and do
comparison and boolean operations against the fields.

The serialization format of `hierarchy` is documented separately in
[Serialization](hierarchy/serialization.md).

## Selva Object

`SelvaObject` is a new Redis data type for storing object-like data in
compute-efficient manner. It can currently store strings, integers, and
doubles natively in the most efficient format for access and computation.
Depending on the use case, the measured performance improvement over
Redis hash type is 20 % - 25 %. It can potentially offer even greater
performance improvement, compared to hashes, in use cases where the data
structure is accessed directly inside a Redis module.

`SelvaObject` stores the object keys in a red-black tree. The key can
store small C-native data types as values directly and have a pointer
to the value for more complex or storage heavy types. The key itself
knows the data type of the value.

The `SelvaObject` data type is implemented in
[selva_object](../module/selva_object.c).

## Selva Node

A Selva node is built of several parts stored in different ways in Redis and
therefore also the management and ownership of the data is divided into several
C files. Typically when a selva node is mentioned it means both a node in the
hierarchy and the companion `SelvaObject` stored in a separate Redis key.

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

## Subscriptions

A subscription is a collection of subscription markers. A subscription marker is
either set to start from a node or it's a trigger marker which is not attached
to any specific node. A third distinct marker type is called the missing accessor
marker that is waiting for a node or alias to be created.

When a regular node marker is created it's applied to the node it starts from and
it's applied to all the connected nodes following the edge constraint of the
specified field or relation. For example a marker could start from `ma0002` and
it could be applied to all the descendants of the node.

A marker will trigger an event on behalf of the subscription if a conjunction of
the given conditions become truthy in certain situations. This could happen when
a field of one of the nodes is changed, a new node matching the marker conditions
is added in the graph, etc.

A Trigger marker will trigger an event based on its trigger type and conditions.

## Commands

### Find Commands

See [find.c](../module/find.c).

### Hierarchy Commands

See [hierarchy.c](../module/hierarchy.c).

### Subscription Commands

See [subscriptions.c](../module/subscriptions.c).

### Modify

Create or update Selva nodes.

See [modify.c](../module/modify.c).

### Inherit

See [inherit.c](../module/inherit.c).

### Selva Objects

See [selva_object](../module/selva_object.c).
