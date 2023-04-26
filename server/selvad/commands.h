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
 * TODO
 * cmd: modinfo
 */
#define CMD_ID_MODINFO 5

/**
 * Show running configuration.
 * module: server
 * cmd: config
 */
#define CMD_ID_CONFIG 6

/**
 * Show malloc stats.
 * module: server
 * cmd: mallocstats
 */
#define CMD_ID_MALLOCSTATS 7

/**
 * Dump a jemalloc prof file.
 * module: server
 * cmd: mallocprofdump
 */
#define CMD_ID_MALLOCPROFDUMP 8

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

#define CMD_ID_FLUSH 16

/**
 * Resolve name to a nodeid.
 * module: db
 * cmd: resolve.nodeid
 */
#define CMD_ID_RESOLVE_NODEID 17

/**
 * Find.
 * module: db
 * cmd: hierarchy.find
 */
#define CMD_ID_HIERARCHY_FIND 18

/**
 * Inherit.
 * module: db
 * cmd: hierarchy.inherit
 */
#define CMD_ID_HIERARCHY_INHERIT 19

/**
 * Aggregate.
 * module: db
 * cmd: hierarchy.aggregate
 */
#define CMD_ID_HIERARCHY_AGGREGATE 20

/**
 * Aggregate in list.
 * module: db
 * cmd: hierarchy.aggregateIn
 */
#define CMD_ID_HIERARCHY_AGGREGATE_IN 21

/**
 * Edge add constraints.
 * module: db
 * cmd: hierarchy.addConstraint
 */
#define CMD_ID_HIERARCHY_ADDCONSTRAINT 22

/**
 * Edge list constraints.
 * module: db
 * cmd: hierarchy.listConstraints
 */
#define CMD_ID_HIERARCHY_LIST_CONSTRAINTS 23

/**
 * Delete node.
 * module: db
 * cmd: hierarchy.del
 */
#define CMD_ID_HIERARCHY_DEL 24

/**
 * List hierarchy heads.
 * module: db
 * cmd: hierarchy.heads
 */
#define CMD_ID_HIERARCHY_HEADS 25

/**
 * List node parents.
 * module: db
 * cmd: hierarchy.parents
 */
#define CMD_ID_HIERARCHY_PARENTS 26

/**
 * List node children.
 * module: db
 * cmd: hierarchy.children
 */
#define CMD_ID_HIERARCHY_CHILDREN 27

/**
 * List node edges.
 * module: db
 * cmd: hierarchy.edgeList
 */
#define CMD_ID_HIERARCHY_EDGE_LIST 28

/**
 * Get node edges.
 * module: db
 * cmd: hierarchy.edgeGet
 */
#define CMD_ID_HIERARCHY_EDGE_GET 29

/**
 * Get node edge metadata.
 * module: db
 * cmd: hierarchy.edgeGetMetadata
 */
#define CMD_ID_HIERARCHY_EDGE_GET_METADATA 30

/**
 * Compress a hierarchy subtree.
 * module: db
 * cmd: hierarchy.compress
 */
#define CMD_ID_HIERARCHY_COMPRESS 31

/**
 * List compressed hierarchy subtrees.
 * module: db
 * cmd: hierarchy.listCompressed
 */
#define CMD_ID_HIERARCHY_LIST_COMPRESSED 32

/**
 * Get hierarchy version information.
 * module: db
 * cmd: hierarchy.ver
 */
#define CMD_ID_HIERARCHY_VER 33

/**
 * Add a new node type.
 * module: db
 * cmd: hierarchy.types.add
 */
#define CMD_ID_HIERARCHY_TYPES_ADD 34

/**
 * Clear all node types.
 * module: db
 * cmd: hierarchy.types.clear
 */
#define CMD_ID_HIERARCHY_TYPES_CLEAR 35

/**
 * List all known node types.
 * module: db
 * cmd: hierarchy.types.list
 */
#define CMD_ID_HIERARCHY_TYPES_LIST 36

/**
 * List find indices.
 * module: db
 * cmd: index.list
 */
#define CMD_ID_INDEX_LIST 37

/**
 * Create a new find index.
 * module: db
 * cmd: index.new
 */
#define CMD_ID_INDEX_NEW 38

/**
 * Delete a find index.
 * module: db
 * cmd: index.del
 */
#define CMD_ID_INDEX_DEL 39

/**
 * Describe a find index.
 * module: db
 * cmd: index.debug
 */
#define CMD_ID_INDEX_DEBUG 40

/**
 * Evaluate an RPN expression into a bool.
 * module: db
 * cmd: rpn.evalBool
 */
#define CMD_ID_RPN_EVAL_BOOL 41

/**
 * Evaluate an RPN expression into a double.
 * module: db
 * cmd: rpn.evalDouble
 */
#define CMD_ID_RPN_EVAL_DOUBLE 42

/**
 * Evaluate an RPN expression into a string.
 * module: db
 * cmd: rpn.evalString
 */
#define CMD_ID_RPN_EVAL_STRING 43

/**
 * Evaluate an RPN expression into a set.
 * module: db
 * cmd: rpn.evalSet
 */
#define CMD_ID_RPN_EVAL_SET 44

/**
 * Delete a node data object field value.
 * module: db
 * cmd: object.del
 */
#define CMD_ID_OBJECT_DEL 45

/**
 * Check if a node data object field exists.
 * module: db
 * cmd: object.exists
 */
#define CMD_ID_OBJECT_EXIST 46

/**
 * Get node data object or field.
 * module: db
 * cmd: object.get
 */
#define CMD_ID_OBJECT_GET 47

/**
 * Increment field value by long long.
 * module: db
 * cmd: object.incrby
 */
#define CMD_ID_OBJECT_INCR_BY 48

/**
 * Increment field value by double.
 * module: db
 * cmd: object.incrbydouble
 */
#define CMD_ID_OBJECT_INCR_BY_DOUBLE 49

/**
 * Get the length of a node data object field value.
 * module: db
 * cmd: object.len
 */
#define CMD_ID_OBJECT_LEN 50

/**
 * Set the value of a node data object field.
 * module: db
 * cmd: object.set
 */
#define CMD_ID_OBJECT_SET 51

/**
 * Get the type of a node data object field.
 * module: db
 * cmd: object.type
 */
#define CMD_ID_OBJECT_TYPE 52

/**
 * Get the metadata associated with a node data object field.
 * module: db
 * cmd: object.getMeta
 */
#define CMD_ID_OBJECT_GETMETA 53

/**
 * Set the metadata associated with a node data object field.
 * module: db
 * cmd: object.setMeta
 */
#define CMD_ID_OBJECT_SETMETA 54

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.add
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_ADD 55

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addAlias
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDALIAS 56

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addMissing
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDMISSING 57

/**
 * Add subscription trigger.
 * module: db
 * cmd: subscriptions.addTrigger
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDTRIGGER 58

/**
 * Refresh subscription.
 * module: db
 * cmd: subscriptions.refresh
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_REFRESH 59

/**
 * List all current subscriptions on this server.
 * module: db
 * cmd: subscriptions.list
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_LIST 60

/**
 * List triggers for missing nodes.
 * module: db
 * cmd: subscriptions.listMissing
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_LISTMISSING 61

/**
 * Describe a subscription or marker.
 * module: db
 * cmd: subscriptions.debug
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_DEBUG 62

/**
 * Delete a subscription.
 * module: db
 * cmd: subscriptions.del
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_DEL 63

/**
 * Delete a subscription marker.
 * module: db
 * cmd: subscriptions.delmarker
 */
#define CMD_ID_SUBSCRIPTIONS_SUBSCRIPTIONS_DELMARKER 64

/**
 * Modify a single node.
 * module: db
 * cmd: modify
 */
#define CMD_ID_MODIFY 65

/**
 * Update nodes using a query.
 * module: db
 * cmd: update
 */
#define CMD_ID_UPDATE 66

/**
 * Start replication stream.
 * module: replication
 * cmd: replicasync
 */
#define CMD_ID_REPLICASYNC 67

/**
 * Set this node as a replica of another node.
 * module: replication
 * cmd: replicaof
 */
#define CMD_ID_REPLICAOF 68

/**
 * Show the current status of the replication module.
 * module: replication
 * cmd: replicainfo
 */
#define CMD_ID_REPLICAINFO 69

/**
 * Replica status message.
 * Sent by a replica to the origin.
 * module: replication
 * cmd: replicastatus
 */
#define CMD_ID_REPLICASTATUS 70

/**
 * Wait for replicas to sync.
 * Waits until all replicas are at current or newer eid (if new sync points are
 * created during the execution of this command).
 * module: replication
 * cmd: replicawait
 */
#define CMD_ID_REPLICAWAIT 71
