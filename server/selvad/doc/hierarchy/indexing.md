<!--
Copyright (c) 2022 SAULX

SPDX-License-Identifier: MIT
-->

# Indexing

The indexing system in Selva is completely automatic from the user's point of
view. The Selva client and server determine together in cooperation which
queries or subqueries should be indexed for the best performance gain, and
on in turn, which queries shouldn't be indexed.

The indexing procedure starts when the client breaks down the AST of a get query
filter into smaller subqueries that form super sets of the assumed end result of
the original query. These subquery filters are called hints. Together the filter
and the traversal direction parameters form an indexing query. The hints are
passed to the server along with the query parameters.

An index in Selva is essentially a subscription and a `SelvaSet` of nodeIds. 
The indexing system utilizes a single subscription which contains markers for
each actual index. Like any subscription marker, an index subscription marker
starts from a single node in the hierarchy and executes a selective RPN filter
expression on each node it visits during the traversal. Every node that passes
the expression filter is added to the resulting SelvaSet.

The subscription markers are created as callback markers, which will call
`update_index()` callback every time something hitting the marker changes. This
keeps the index result sets up-to-date most of the time, although on some
occasions the result set must be rebuilt and during those times the index is
unusable.

The indexing result sets can be used by find queries to speed up finding the
result of the query. A find query may include an optional indexing hint
filter (see [RPN expression](expressions.md)) that can be used to form an
index. The hints are cached in a `SelvaObject` and the ones that are seen
frequently are turned into an actual indexing entry with a result set.

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

## Testing

This repository includes functional and performance tests for the dynamic
indexing subsystem. The tests are implemented using the client side test
framework. The test must be run from the client [directory](/client)
using `npm` or `yarn`.

To run one or more tests, execute the following command:

```
yarn test [filename]
```

Below is a list of currently implemented tests for the dynamic indexing
subsystem.

| Test                              | Description                                                                   |
|-----------------------------------|-------------------------------------------------------------------------------|
| `test/findIndexHint.ts`           | Test plain index hints without utilizing much of the client functionality.    |
| `test/findWithIndexes.ts`         | Test indexing using all the client functionality.                             |
| `test/perf/indexing.ts`           | Test indexing performance in real-world-like scenario using synthetic data but without utilizing the normal client side functionality. |

The [indexing performance test](/test/perf/indexing.ts) will create a CSV file
in the [client directory](/client) when executed. This CSV can be used to
further analyze the time complexity of the indexing subsystem as the data set
and index size grows.

Naturally, it's also possible to attach [Intel VTune](/doc/debugging.md#intel-vtune)
to the `redis-server-selva` process while the test is running and gather even
more performance data. The only thing to keep in mind is that the test will run
three Redis processes which of only one is processing the data, the `origin`
process.
