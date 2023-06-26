/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * Traditional ping/pong.
 * module: server
 * cmd: ping
 */
#define CMD_ID_PING 0

/**
 * Echoes back the same strings that were present in the request.
 * module: server
 * cmd: echo
 */
#define CMD_ID_ECHO 1

/**
 * List all currently registered commands.
 * Can be used for commands discovery.
 * module: server
 * cmd: lscmd
 */
#define CMD_ID_LSCMD 2

/**
 * Start a server heartbeat.
 * module: server
 * cmd: hrt
 */
#define CMD_ID_HRT 3

/**
 * List supported languages.
 * module: db
 * cmd: lslang
 */
#define CMD_ID_LSLANG 4

/**
 * List loaded modules.
 * module: server
 * cmd: lsmod
 */
#define CMD_ID_LSMOD 5

/**
 * Show running configuration.
 * module: server
 * cmd: config
 */
#define CMD_ID_CONFIG 6

/**
 * Set or get loglevel.
 * module: server
 * cmd: loglevel
 */
#define CMD_ID_LOGLEVEL 7

/**
 * Toggle debug messages.
 * module: server
 * cmd: dbg
 */
#define CMD_ID_DBG 8

/**
 * Show malloc stats.
 * module: server
 * cmd: mallocstats
 */
#define CMD_ID_MALLOCSTATS 9

/**
 * Dump a jemalloc prof file.
 * module: server
 * cmd: mallocprofdump
 */
#define CMD_ID_MALLOCPROFDUMP 10

/**
 * Load db.
 * module: db
 * cmd: load
 */
#define CMD_ID_LOAD 14

/**
 * Save db.
 * module: db
 * cmd: save
 */
#define CMD_ID_SAVE 15

/**
 * Flush the in-mem database.
 * module: db
 * cmd: flush
 */
#define CMD_ID_FLUSH 16

/**
 * Purge old SDB dumps.
 * module: db
 * cmd: purge
 */
#define CMD_ID_PURGE 17

/**
 * Resolve name to a nodeid.
 * module: db
 * cmd: resolve.nodeid
 */
#define CMD_ID_RESOLVE_NODEID 18

/**
 * Find.
 * module: db
 * cmd: hierarchy.find
 */
#define CMD_ID_HIERARCHY_FIND 19

/**
 * Inherit.
 * module: db
 * cmd: hierarchy.inherit
 */
#define CMD_ID_HIERARCHY_INHERIT 20

/**
 * Aggregate.
 * module: db
 * cmd: hierarchy.aggregate
 */
#define CMD_ID_HIERARCHY_AGGREGATE 21

/**
 * Aggregate in list.
 * module: db
 * cmd: hierarchy.aggregateIn
 */
#define CMD_ID_HIERARCHY_AGGREGATE_IN 22

/**
 * Edge add constraints.
 * module: db
 * cmd: hierarchy.addConstraint
 */
#define CMD_ID_HIERARCHY_ADDCONSTRAINT 23

/**
 * Edge list constraints.
 * module: db
 * cmd: hierarchy.listConstraints
 */
#define CMD_ID_HIERARCHY_LIST_CONSTRAINTS 24

/**
 * Delete node.
 * module: db
 * cmd: hierarchy.del
 */
#define CMD_ID_HIERARCHY_DEL 25

/**
 * List hierarchy heads.
 * module: db
 * cmd: hierarchy.heads
 */
#define CMD_ID_HIERARCHY_HEADS 26

/**
 * List node parents.
 * module: db
 * cmd: hierarchy.parents
 */
#define CMD_ID_HIERARCHY_PARENTS 27

/**
 * List node children.
 * module: db
 * cmd: hierarchy.children
 */
#define CMD_ID_HIERARCHY_CHILDREN 28

/**
 * List node edges.
 * module: db
 * cmd: hierarchy.edgeList
 */
#define CMD_ID_HIERARCHY_EDGE_LIST 29

/**
 * Get node edges.
 * module: db
 * cmd: hierarchy.edgeGet
 */
#define CMD_ID_HIERARCHY_EDGE_GET 30

/**
 * Get node edge metadata.
 * module: db
 * cmd: hierarchy.edgeGetMetadata
 */
#define CMD_ID_HIERARCHY_EDGE_GET_METADATA 31

/**
 * Compress a hierarchy subtree.
 * module: db
 * cmd: hierarchy.compress
 */
#define CMD_ID_HIERARCHY_COMPRESS 32

/**
 * List compressed hierarchy subtrees.
 * module: db
 * cmd: hierarchy.listCompressed
 */
#define CMD_ID_HIERARCHY_LIST_COMPRESSED 33

/**
 * Get hierarchy version information.
 * module: db
 * cmd: hierarchy.ver
 */
#define CMD_ID_HIERARCHY_VER 34

/**
 * Add a new node type.
 * module: db
 * cmd: hierarchy.types.add
 */
#define CMD_ID_HIERARCHY_TYPES_ADD 35

/**
 * Clear all node types.
 * module: db
 * cmd: hierarchy.types.clear
 */
#define CMD_ID_HIERARCHY_TYPES_CLEAR 36

/**
 * List all known node types.
 * module: db
 * cmd: hierarchy.types.list
 */
#define CMD_ID_HIERARCHY_TYPES_LIST 37

/**
 * List find indices.
 * module: db
 * cmd: index.list
 */
#define CMD_ID_INDEX_LIST 38

/**
 * Create a new find index.
 * module: db
 * cmd: index.new
 */
#define CMD_ID_INDEX_NEW 39

/**
 * Delete a find index.
 * module: db
 * cmd: index.del
 */
#define CMD_ID_INDEX_DEL 40

/**
 * Describe a find index.
 * module: db
 * cmd: index.debug
 */
#define CMD_ID_INDEX_DEBUG 41

/**
 * Indexing info.
 * module: db
 * cmd: index.debug
 */
#define CMD_ID_INDEX_INFO 42

/**
 * Evaluate an RPN expression into a bool.
 * module: db
 * cmd: rpn.evalBool
 */
#define CMD_ID_RPN_EVAL_BOOL 43

/**
 * Evaluate an RPN expression into a double.
 * module: db
 * cmd: rpn.evalDouble
 */
#define CMD_ID_RPN_EVAL_DOUBLE 44

/**
 * Evaluate an RPN expression into a string.
 * module: db
 * cmd: rpn.evalString
 */
#define CMD_ID_RPN_EVAL_STRING 45

/**
 * Evaluate an RPN expression into a set.
 * module: db
 * cmd: rpn.evalSet
 */
#define CMD_ID_RPN_EVAL_SET 46

/**
 * Delete a node data object field value.
 * module: db
 * cmd: object.del
 */
#define CMD_ID_OBJECT_DEL 47

/**
 * Check if a node data object field exists.
 * module: db
 * cmd: object.exists
 */
#define CMD_ID_OBJECT_EXISTS 48

/**
 * Get node data object or field.
 * module: db
 * cmd: object.get
 */
#define CMD_ID_OBJECT_GET 49

/**
 * Increment field value by long long.
 * module: db
 * cmd: object.incrby
 */
#define CMD_ID_OBJECT_INCRBY 50

/**
 * Increment field value by double.
 * module: db
 * cmd: object.incrbydouble
 */
#define CMD_ID_OBJECT_INCRBY_DOUBLE 51

/**
 * Get the length of a node data object field value.
 * module: db
 * cmd: object.len
 */
#define CMD_ID_OBJECT_LEN 52

/**
 * Set the value of a node data object field.
 * module: db
 * cmd: object.set
 */
#define CMD_ID_OBJECT_SET 53

/**
 * module: db
 * cmd: object.keys
 */
#define CMD_ID_OBJECT_KEYS 54

/**
 * Get the type of a node data object field.
 * module: db
 * cmd: object.type
 */
#define CMD_ID_OBJECT_TYPE 55

/**
 * Get the metadata associated with a node data object field.
 * module: db
 * cmd: object.getMeta
 */
#define CMD_ID_OBJECT_GETMETA 56

/**
 * Set the metadata associated with a node data object field.
 * module: db
 * cmd: object.setMeta
 */
#define CMD_ID_OBJECT_SETMETA 57

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.add
 */
#define CMD_ID_SUBSCRIPTIONS_ADD 58

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addAlias
 */
#define CMD_ID_SUBSCRIPTIONS_ADDALIAS 59

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addMissing
 */
#define CMD_ID_SUBSCRIPTIONS_ADDMISSING 60

/**
 * Add subscription trigger.
 * module: db
 * cmd: subscriptions.addTrigger
 */
#define CMD_ID_SUBSCRIPTIONS_ADDTRIGGER 61

/**
 * Refresh subscription.
 * module: db
 * cmd: subscriptions.refresh
 */
#define CMD_ID_SUBSCRIPTIONS_REFRESH 62

/**
 * List all current subscriptions on this server.
 * module: db
 * cmd: subscriptions.list
 */
#define CMD_ID_SUBSCRIPTIONS_LIST 63

/**
 * List triggers for missing nodes.
 * module: db
 * cmd: subscriptions.listMissing
 */
#define CMD_ID_SUBSCRIPTIONS_LISTMISSING 64

/**
 * Describe a subscription or marker.
 * module: db
 * cmd: subscriptions.debug
 */
#define CMD_ID_SUBSCRIPTIONS_DEBUG 65

/**
 * Delete a subscription.
 * module: db
 * cmd: subscriptions.del
 */
#define CMD_ID_SUBSCRIPTIONS_DEL 66

/**
 * Delete a subscription marker.
 * module: db
 * cmd: subscriptions.delmarker
 */
#define CMD_ID_SUBSCRIPTIONS_DELMARKER 67

/**
 * Modify a single node.
 * module: db
 * cmd: modify
 */
#define CMD_ID_MODIFY 68

/**
 * Update nodes using a query.
 * module: db
 * cmd: update
 */
#define CMD_ID_UPDATE 69

/**
 * List node aliases.
 * module: db
 * cmd: lsaliases
 */
#define CMD_ID_LSALIASES 70

/**
 * Start replication stream.
 * module: replication
 * cmd: replicasync
 */
#define CMD_ID_REPLICASYNC 71

/**
 * Set this node as a replica of another node.
 * module: replication
 * cmd: replicaof
 */
#define CMD_ID_REPLICAOF 72

/**
 * Show the current status of the replication module.
 * module: replication
 * cmd: replicainfo
 */
#define CMD_ID_REPLICAINFO 73

/**
 * Replica status message.
 * Sent by a replica to the origin.
 * module: replication
 * cmd: replicastatus
 */
#define CMD_ID_REPLICASTATUS 74

/**
 * Wait for replicas to sync.
 * Waits until all replicas are at current or newer eid (if new sync points are
 * created during the execution of this command).
 * module: replication
 * cmd: replicawait
 */
#define CMD_ID_REPLICAWAIT 75

/**
 * Publish a message to a channel.
 * module: server
 * cmd: publish
 */
#define CMD_ID_PUBLISH 76

/**
 * Subscribe to a channel.
 * module: server
 * cmd: subscribe
 */
#define CMD_ID_SUBSCRIBE 77

/**
 * Unsubscribe from a channel.
 * module: server
 * cmd: unsubscribe
 */
#define CMD_ID_UNSUBSCRIBE 78
