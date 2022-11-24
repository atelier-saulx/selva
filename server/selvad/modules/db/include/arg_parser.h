/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once
#ifndef SELVA_ARG_PARSER
#define SELVA_ARG_PARSER

#include "selva_db.h"

struct SelvaObject;
struct finalizer;
struct selva_string;

typedef struct selva_string ** selva_stringList;

struct SelvaArgParser_EnumType {
    char *name;
    int id;
};

/**
 * Parse a selva proto buffer into selva_strings.
 * Parse and flatten a message buffer `buf` containing only strings and string
 * arrays into an array of selva_string pointers.
 * Returned strings may be removed from the finalizer `fin` individually.
 * Also the output list `out` is added to the finalizer.
 * @param buf is a message buffer supposed to contain selva proto values.
 * @param bsize is the size of buf in bytes.
 * @param[out] out is pointer to the variable that will store the array of selva_string pointers.
 * @returns If successful, returns the number of strings in out; Otherwise an error code is returned.
 */
int SelvaArgParser_buf2strings(struct finalizer *fin, const char *buf, size_t bsize, selva_stringList *out);

/**
 * Parse an option with key string.
 * TXT INT_VALUE
 * @param value[out] pointer to a storage for the parser ssize_t value.
 * @param name a pointer to the key string.
 * @param txt a pointer to the key string argument.
 * @param num a pointer to the value string argument.
 */
int SelvaArgParser_IntOpt(
        ssize_t *value, const char *name,
        const struct selva_string *txt,
        const struct selva_string *num);

int SelvaArgParser_StrOpt(
        const char **value,
        const char *name,
        const struct selva_string *arg_key,
        const struct selva_string *arg_val);
int SelvaArgsParser_StringList(
        struct finalizer *finalizer,
        selva_stringList *out,
        const char *name,
        const struct selva_string *arg_key,
        const struct selva_string *arg_val);

/**
 * Parse a set of lists containing strings.
 * Exclusion prefix: '!'
 * Set separator: '\n'
 * List separator: '|'
 * Enf of sets: '\0'
 * The list_out object will be built as follows:
 * {
 *   '0': ['field1', 'field2'],
 *   '1': ['field3', 'field4'],
 * }
 */
int SelvaArgsParser_StringSetList(
        struct finalizer *finalizer,
        struct SelvaObject **list_out,
        struct selva_string **excluded_out,
        const char *name,
        const struct selva_string *arg_key,
        const struct selva_string *arg_val);

int SelvaArgParser_Enum(
        const struct SelvaArgParser_EnumType types[],
        const struct selva_string *arg);
int SelvaArgParser_NodeType(
        Selva_NodeType node_type,
        const struct selva_string *arg);
int SelvaArgParser_SubscriptionId(
        Selva_SubscriptionId id,
        const struct selva_string *arg);
int SelvaArgParser_MarkerId(
        Selva_SubscriptionMarkerId *marker_id,
        const struct selva_string *arg);
/**
 * Parse index hints from Redis command args.
 * Parses index hints from argv until the first keyword mismatch.
 * @returns The number of index hints found.
 */
int SelvaArgParser_IndexHints(
        selva_stringList *out,
        struct selva_string **argv,
        int argc);

#endif /* SELVA_ARG_PARSER */
