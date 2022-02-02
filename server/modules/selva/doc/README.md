# Selva Redis Module

## Hierarchy

Hierarchy is an acyclic graph data structure used for storing the
hierarchical relationships between the nodes in Selva. Hierarchy
implements parent and child constraints that have a very specific
behavior when executing certain operations on the hierarchy.

Hierarchy implements a number of different ways to traverse the graph
utilizing not only the family tree -like relationships but also other
possible reference and relationship types. The traversal methods can
also filter out nodes from the results by using
[RPN Filter Expressions](hierarchy/expressions.md) with the traversal
methods, which can, most importantly, access the node fields and do
comparison and boolean operations against the fields.

The serialization format of `hierarchy` is documented separately in
[Serialization](hierarchy/serialization.md).

## Selva Object

`SelvaObject` is a new data type for storing object-like data in
compute-efficient manner. In addition to in-memory storage it can
be also serialized and deserialized. The type can currently store strings,
integers, and doubles natively in the most efficient format for access and
computation.  Depending on the use case, the measured performance improvement
over Redis hash type is 20 % - 25 %. It can potentially offer even greater
performance improvement, compared to hashes, in use cases where the data
structure is accessed directly inside a Redis module.

`SelvaObject` stores the object keys in a rank-balanced tree. The key can
store small C-native data types as values directly and have a pointer
to the value for more complex or storage heavy types. The key itself
knows the data type of the value.

The `SelvaObject` data type is implemented in
[selva_object](../module/selva_object.c).

## Selva Node

A Selva node is a structure managed by the hierarchy system. A node
contains hard-coded hierarchy fields (`parents` and `children`),
a `SelvaObject` containing the data fields, edge fields, and other meta
data. Edge fields are actually handled as an external metadata to the
hierarchy system.

Hierarchy fields `ancestors`, `children`, `descendants`, and `parents` are
owned, managed, and traversed by the functions in
[hierarchy.c](../module/hierarchy.c).

Node aliases (and `aliases` field) are handled by [alias.c](../module/alias.c).

## Graph Link Types

The Selva C module supports a number ways to link or reference to other nodes in
the database.

### Parents/Children

The most important graph reference type in Selva is the parent/child
relationship. This relationship connects selva nodes together typically in
a tree manner but other topologies can be created by adjusting the
relationships.

This link type is fully managed and every edge must have an existing endpoint.
The relationship is always symmetric, if a node has a parent then it's also a
child to the parent. In addition to these constraints, in normal operation,
if a node or subtree is left orphan, it will be destroyed.

### Reference

Another way to point to other nodes is to create a reference field that
contains a set of nodeIds. These ids don't need to exist at the time of
adding to the set, but if they do the nodes will be visited when the field
is traversed.

This reference type is completely unmanaged and nothing will happen if a
referenced node is removed.

### Edge Field

Edge fields are customizable user defined edge types in the Selva C module.
An edge field is tied to a user defined edge constraint, that mandates the
behavior of the edges of the field on creation, traversals, and other
database operations. This is the reference type that makes Selva a true graph
database.

These fields are managed according to the selected edge constraint, and
an edge can only exist if its endpoint exists.

The edge field system is documented [here](edge.md).

## Subscriptions

A subscription is a collection of subscription markers. There are three basic
types of subscription markers. A regular marker is attached to a start node and
the nodes that follow it according to the rules of the traversal specified, and
the marker reacts to changes in the node's relationships or data.  A missing
accessor marker is a marker that is waiting for a node or an alias to appear
(to be created). Finally a trigger marker reacts to certain database events.

The reality with the markers is a bit more complex than what was just stated,
because the actual marker types can be mixed with matcher flags and action
functions can change the actual behavior. Some markers are also detached and
not directly attached to a node or nodeId, one example of such a maker is the
trigger marker.

When a regular node marker is created it's applied to the node it starts from and
it's applied to all the connected nodes following the edge constraint of the
specified field or relation. For example a marker could start from `ma0002` and
it could be applied to all the descendants of the node.

A marker will trigger an event on behalf of the subscription if a conjunction of
the given conditions become truthy in certain situations. This could happen when
a field of one of the nodes is changed, a new node matching the marker conditions
is added in the graph, etc.

A Trigger marker will trigger an event based on its trigger type and conditions.

## Indexing

Traversals over the hierarchy can be long and complex and additional RPN
expression filtering can make it slow. Often subscriptions require a new
traversal to be executed every time something changes in the subscription.
To help speeding up the most complex and frequent traversals, there is a
built-in indexing support, that can be utilized by providing indexing hints
when executing a find query. For efficient indexing, an indexing hint should
form a proper super set of the normal find query result.

See [Indexing](hierarchy/indexing.md).

## Commands

### Find Commands

See

- [find.c](../module/find.c),
- [aggregate.c](../module/aggregate.c),
- [inherit.c](../module/inherit.c).

### Hierarchy Commands

Direct hierarchy manipulation.

See [hierarchy.c](../module/hierarchy.c).

### Subscription Commands

See [subscriptions.c](../module/subscriptions.c).

### Modify

Create or update Selva nodes.

See [modify.c](../module/modify.c).

### Selva Objects

Direct SelvaObject manipulation for objects backed by a Redis key.

See [selva_object](../module/selva_object.c).
