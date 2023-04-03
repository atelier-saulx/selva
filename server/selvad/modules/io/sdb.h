/*
 * Copyright (c) 2022-2023 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

void sdb_init(struct selva_io *io);

int sdb_write_header(struct selva_io *io);
int sdb_read_header(struct selva_io *io);
int sdb_write_footer(struct selva_io *io);
int sdb_read_footer(struct selva_io *io);
