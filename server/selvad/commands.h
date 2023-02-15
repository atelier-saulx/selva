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
#define CMD_PING_ID 0

/**
 * Echoes back the same strings that were present in the request.
 * module: server
 * cmd: echo
 */
#define CMD_ECHO_ID 1

/**
 * List all currently registered commands.
 * Can be used for commands discovery.
 * module: server
 * cmd: lscmd
 */
#define CMD_LSCMD_ID 2

/**
 * Start a server heartbeat.
 * module: server
 * cmd: hrt
 */
#define CMD_HRT_ID 3

/**
 * List supported languages.
 * module: db
 * cmd: lslang
 */
#define CMD_LSLANG_ID 4

/**
 * TODO
 * cmd: modinfo
 */
#define CMD_MODINFO_ID 5

/**
 * Load db.
 * module: db
 * cmd: load
 */
#define CMD_LOAD_ID 14

/**
 * Save db.
 * module: db
 * cmd: save
 */
#define CMD_SAVE_ID 15

/**
 * Resolve name to a nodeid.
 * module: db
 * cmd: resolve.nodeid
 */
#define CMD_RESOLVE_NODEID_ID 16

/**
 * Find.
 * module: db
 * cmd: hierarchy.find
 */
#define CMD_HIERARCHY_FIND_ID 17

/**
 * Inherit.
 * module: db
 * cmd: hierarchy.inherit
 */
#define CMD_HIERARCHY_INHERIT_ID 18

/**
 * Aggregate.
 * module: db
 * cmd: hierarchy.aggregate
 */
#define CMD_HIERARCHY_AGGREGATE_ID 19

/**
 * Aggregate in list.
 * module: db
 * cmd: hierarchy.aggregateIn
 */
#define CMD_HIERARCHY_AGGREGATE_IN_ID 20

/**
 * Edge add constraints.
 * module: db
 * cmd: hierarchy.addConstraint
 */
#define CMD_HIERARCHY_ADDCONSTRAINT_ID 21

/**
 * Edge list constraints.
 * module: db
 * cmd: hierarchy.listConstraints
 */
#define CMD_HIERARCHY_LIST_CONSTRAINTS_ID 22

/**
 * Delete node.
 * module: db
 * cmd: hierarchy.del
 */
#define CMD_HIERARCHY_DEL_ID 23

/**
 * List hierarchy heads.
 * module: db
 * cmd: hierarchy.heads
 */
#define CMD_HIERARCHY_HEADS_ID 24

/**
 * List node parents.
 * module: db
 * cmd: hierarchy.parents
 */
#define CMD_HIERARCHY_PARENTS_ID 25

/**
 * List node children.
 * module: db
 * cmd: hierarchy.children
 */
#define CMD_HIERARCHY_CHILDREN_ID 26

/**
 * List node edges.
 * module: db
 * cmd: hierarchy.edgeList
 */
#define CMD_HIERARCHY_EDGE_LIST_ID 27

/**
 * Get node edges.
 * module: db
 * cmd: hierarchy.edgeGet
 */
#define CMD_HIERARCHY_EDGE_GET_ID 28

/**
 * Get node edge metadata.
 * module: db
 * cmd: hierarchy.edgeGetMetadata
 */
#define CMD_HIERARCHY_EDGE_GET_METADATA_ID 29

/**
 * Compress a hierarchy subtree.
 * module: db
 * cmd: hierarchy.compress
 */
#define CMD_HIERARCHY_COMPRESS_ID 30

/**
 * List compressed hierarchy subtrees.
 * module: db
 * cmd: hierarchy.listCompressed
 */
#define CMD_HIERARCHY_LIST_COMPRESSED_ID 31

/**
 * Get hierarchy version information.
 * module: db
 * cmd: hierarchy.ver
 */
#define CMD_HIERARCHY_VER_ID 32

/**
 * Add a new node type.
 * module: db
 * cmd: hierarchy.types.add
 */
#define CMD_HIERARCHY_TYPES_ADD_ID 33

/**
 * Clear all node types.
 * module: db
 * cmd: hierarchy.types.clear
 */
#define CMD_HIERARCHY_TYPES_CLEAR_ID 34

/**
 * List all known node types.
 * module: db
 * cmd: hierarchy.types.list
 */
#define CMD_HIERARCHY_TYPES_LIST_ID 35

/**
 * List find indices.
 * module: db
 * cmd: index.list
 */
#define CMD_INDEX_LIST_ID 36

/**
 * Create a new find index.
 * module: db
 * cmd: index.new
 */
#define CMD_INDEX_NEW_ID 37

/**
 * Delete a find index.
 * module: db
 * cmd: index.del
 */
#define CMD_INDEX_DEL_ID 38

/**
 * Describe a find index.
 * module: db
 * cmd: index.debug
 */
#define CMD_INDEX_DEBUG_ID 39

/**
 * Evaluate an RPN expression into a bool.
 * module: db
 * cmd: rpn.evalBool
 */
#define CMD_RPN_EVAL_BOOL_ID 41

/**
 * Evaluate an RPN expression into a double.
 * module: db
 * cmd: rpn.evalDouble
 */
#define CMD_RPN_EVAL_DOUBLE_ID 42

/**
 * Evaluate an RPN expression into a string.
 * module: db
 * cmd: rpn.evalString
 */
#define CMD_RPN_EVAL_STRING_ID 43

/**
 * Evaluate an RPN expression into a set.
 * module: db
 * cmd: rpn.evalSet
 */
#define CMD_RPN_EVAL_SET_ID 44

/**
 * Delete a node data object field value.
 * module: db
 * cmd: object.del
 */
#define CMD_OBJECT_DEL_ID 45

/**
 * Check if a node data object field exists.
 * module: db
 * cmd: object.exists
 */
#define CMD_OBJECT_EXIST_ID 46

/**
 * Get node data object or field.
 * module: db
 * cmd: object.get
 */
#define CMD_OBJECT_GET_ID 47

/**
 * Get the length of a node data object field value.
 * module: db
 * cmd: object.len
 */
#define CMD_OBJECT_LEN_ID 48

/**
 * Set the value of a node data object field.
 * module: db
 * cmd: object.set
 */
#define CMD_OBJECT_SET_ID 49

/**
 * Get the type of a node data object field.
 * module: db
 * cmd: object.type
 */
#define CMD_OBJECT_TYPE_ID 50

/**
 * Get the metadata associated with a node data object field.
 * module: db
 * cmd: object.getMeta
 */
#define CMD_OBJECT_GETMETA_ID 51

/**
 * Set the metadata associated with a node data object field.
 * module: db
 * cmd: object.setMeta
 */
#define CMD_OBJECT_SETMETA_ID 52

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.add
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_ADD_ID 53

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addAlias
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDALIAS_ID 54

/**
 * Add subscription marker.
 * module: db
 * cmd: subscriptions.addMissing
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDMISSING_ID 55

/**
 * Add subscription trigger.
 * module: db
 * cmd: subscriptions.addTrigger
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_ADDTRIGGER_ID 56

/**
 * Refresh subscription.
 * module: db
 * cmd: subscriptions.refresh
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_REFRESH_ID 57

/**
 * List all current subscriptions on this server.
 * module: db
 * cmd: subscriptions.list
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_LIST_ID 58

/**
 * List triggers for missing nodes.
 * module: db
 * cmd: subscriptions.listMissing
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_LISTMISSING_ID 59

/**
 * Describe a subscription or marker.
 * module: db
 * cmd: subscriptions.debug
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_DEBUG_ID 60

/**
 * Delete a subscription.
 * module: db
 * cmd: subscriptions.del
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_DEL_ID 61

/**
 * Delete a subscription marker.
 * module: db
 * cmd: subscriptions.delmarker
 */
#define CMD_SUBSCRIPTIONS_SUBSCRIPTIONS_DELMARKER_ID 62

/**
 * Modify a single node.
 * module: db
 * cmd: modify
 */
#define CMD_MODIFY_ID 63

/**
 * Update nodes using a query.
 * module: db
 * cmd: update
 */
#define CMD_UPDATE_ID 64

/**
 * Initialize this node as an origin.
 * module: replication
 * cmd: replicainit
 */
#define CMD_REPLICAINIT_ID 65

/**
 * Start replication stream.
 * module: replication
 * cmd: replicasync
 */
#define CMD_REPLICASYNC_ID 66

/**
 * Set this node as a replica of another node.
 * module: replication
 * cmd: replicaof
 */
#define CMD_REPLICAOF_ID 67

/**
 * Show the current status of the replication module.
 * module: replication
 * cmd: replicainfo
 */
#define CMD_REPLICAINFO_ID 68
