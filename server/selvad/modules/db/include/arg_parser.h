/*
 * Copyright (c) 2022-2023 SAULX
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
