/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void sdb_init(struct selva_io *io);
size_t sdb_write(const void * ptr, size_t size, size_t count, struct selva_io *io);
size_t sdb_read(void * ptr, size_t size, size_t count, struct selva_io *io);
int sdb_write_header(struct selva_io *io);
int sdb_read_header(struct selva_io *io);
int sdb_write_footer(struct selva_io *io);
int sdb_read_footer(struct selva_io *io);
