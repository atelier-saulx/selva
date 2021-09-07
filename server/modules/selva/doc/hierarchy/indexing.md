# Indexing

An index is essentially a subscription and a SelvaSet of nodeIds. The indexing
subscription, like any subscription, starts from a node in the hierarchy and
executes a selective RPN filter expression on each node it visits during the
traversal. Every node that passes the expression filter is added to the
resulting SelvaSet.

The indexing system utilizes a single subscription which contains markers for
each index. The subscription markers are created as callback markers, which
will call `update_index()` callback every time something hitting the marker
changes. This keeps the index result sets up-to-date most of the time, although
on some occasions the result set must be rebuilt and during those times the
index is unusable.

The indexing result sets can be used by find queries to speed up finding the
result of the query. A find query can include an optional indexing hint
filter that can be used to form an index. The hints are cached in a
`SelvaObject` and the ones that are seen frequently are turned into an actual
indexing entry with a result set.

Formally, the index is a map from the tuple
`(node_id, direction[, dir_expression], indexing_clause)` to a set of
hierarchy nodeIds.

When a previously unseen indexing hint is observed a new `IndexControlBlock` or
`icb` is created. If further on the same hint is seen often enough a new index
is created. Once the new index is in `valid` state, the find command can use the
new index result set as a starting point for building the result of the find
query, instead of traversing the hierarchy to find the relevant nodes. This will
work as long as the index result set is a super set of what would normally be
the result set of the find query when no indexing is used.

The `IndexControlBlock` structure has a number of state flags which are needed
to determine whether the indexing mechanism is active and the result set is
valid.

| State flag            | Description                                                                                                                                                   |
|-----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `is_valid_marked_id`  | A subscription marker id is reserved for this `icb` but it's not necessary in use yet (in the subscription).                                                  |
| `is_valid_timer_id`   | There is a timer running for state updates. (Generally this is always happening when there is an `icb` for a hint.)                                           |
| `unsigned is_active`  | Indexing is active. The index res set is actively being updated although res can be invalid from time to time, meaning that the index is currently invalid.   |
| `is_valid`            | The result set is valid, i.e. contains all the nodeIds that should be there and the result can be used by the find command.                                   |
