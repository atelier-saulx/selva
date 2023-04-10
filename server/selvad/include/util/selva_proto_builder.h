/*
 * Copyright (c) 2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_proto_builder_msg {
    size_t bsize;
    size_t nr_values; /*!< Total number of values in the buffer. */
    uint8_t *buf;
};

/**
 * Initialize a proto message builder struct.
 * This function allocates msg->buf in a way that it can be freed with either
 * using selva_proto_builder_deinit() or optionally with selva_free().
 */
void selva_proto_builder_init(struct selva_proto_builder_msg *msg);
void selva_proto_builder_end(struct selva_proto_builder_msg *msg);
void selva_proto_builder_deinit(struct selva_proto_builder_msg *msg);
void selva_proto_builder_insert_null(struct selva_proto_builder_msg *msg);
void selva_proto_builder_insert_double(struct selva_proto_builder_msg *msg, double v);
void selva_proto_builder_insert_longlong(struct selva_proto_builder_msg *msg, long long v);
void selva_proto_builder_insert_string(struct selva_proto_builder_msg * restrict msg, const char * restrict str, size_t len);
void selva_proto_builder_insert_array(struct selva_proto_builder_msg *msg);
void selva_proto_builder_insert_array_end(struct selva_proto_builder_msg *msg);
