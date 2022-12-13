/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_io {
    enum selva_io_flags flags;
    FILE *file;
    sha3_context hash_c; /*!< Currently computed hash of the data. */
};

size_t sdb_write(const void * ptr, size_t size, size_t count, struct selva_io *io);
size_t sdb_read(void * ptr, size_t size, size_t count, struct selva_io *io);
int sdb_write_header(struct selva_io *io);
int sdb_read_header(struct selva_io *io);
int sdb_write_footer(struct selva_io *io);
int sdb_read_footer(struct selva_io *io);
