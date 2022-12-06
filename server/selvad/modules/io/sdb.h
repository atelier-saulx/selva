/*
 * Copyright (c) 2022 SAULX
 * SPDX-License-Identifier: MIT
 */
#pragma once

struct selva_io {
    enum selva_io_flags flags;
    FILE *file;
    char hash[16]; /*!< Currently computed hash of the data. */
    int err; /*!< Store error. If set the saving can be halted/skipped. RFE needed? */
};

int fwrite_sdb_header(struct selva_io *io);
int fread_sdb_header(struct selva_io *io);
int fwrite_sdb_footer(struct selva_io *io);
int fread_sdb_footer(struct selva_io *io);
